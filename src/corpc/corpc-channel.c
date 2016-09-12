/*
 * corpc-channel.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */


#include <cuttle/debug.h>
#include "corpc-channel.h"
#include "corpc-listening-port.h"
#include "corpc-proto.h"
#include <errno.h>

#define CORPC_CHANNEL_THREAD_STACK_SIZE   (1024*1024)
#define CORPC_STREAM_DEFAULT_QUEUE_SIZE   8




const char * corpc_channel_state_string(enum corpc_channel_state state)
{
  switch (state) {
    case corpc_channel_state_idle:
      return "idle";
    case corpc_channel_state_connecting:
      return "connecting";
    case corpc_channel_state_connected:
      return "connected";
    case corpc_channel_state_accepting:
      return "accepting";
    case corpc_channel_state_accepted:
      return "accepted";
    case corpc_channel_state_disconnecting:
      return "disconnecting";
    default:
    break;
  }
  return "bug:invalid-channel-state";
}

const char * corpc_stream_state_string(enum corpc_stream_state state)
{
  switch ( state ) {
    case corpc_stream_state_idle :
      return "idle";
    case corpc_stream_state_opening :
      return "opening";
    case corpc_stream_state_opened :
      return "opened";
    case corpc_stream_state_closing :
      return "closing";
    case corpc_stream_state_closed :
      return "closed";
    default :
      break;
  }
  return "bug:invalid-stream-state";
}


void corpc_stream_cleanup(struct corpc_stream * st)
{
  ccfifo_cleanup(&st->rxq);
  memset(st, 0, sizeof(*st));
}

bool corpc_stream_init(struct corpc_stream * st, const corpc_stream_opts * args)
{
  bool fok = false;

  memset(st, 0, sizeof(*st));

  if ( ccfifo_init(&st->rxq, CORPC_STREAM_DEFAULT_QUEUE_SIZE, sizeof(comsg*)) ) {
    st->channel = args->channel;
    st->sid = args->sid;
    st->did = args->did;
    st->state = args->state;
    fok = true;
  }

  return fok;
}

void corpc_stream_destroy(corpc_stream ** st)
{
  if ( st && *st ) {
    corpc_stream_cleanup(*st);
    free(*st);
    *st = NULL;
  }
}

corpc_stream * corpc_stream_new(const corpc_stream_opts * args)
{
  corpc_stream * st = NULL;
  bool fok = false;

  if ( !(st = malloc(sizeof(*st))) ) {
    goto end;
  }

  if ( !corpc_stream_init(st, args) ) {
    goto end;
  }

  fok = true;

end:

  if ( !fok && st ) {
    corpc_stream_destroy(&st);
  }

  return st;
}

enum corpc_stream_state corpc_get_stream_state(const corpc_stream * st)
{
  return st->state;
}

void corpc_set_stream_state(struct corpc_stream * st, corpc_stream_state state)
{
  st->state = state;
}


bool corpc_stream_write(struct corpc_stream * st, const corpc_msg * msg)
{
  return corpc_channel_write(st, msg);
}

bool corpc_stream_read(struct corpc_stream * st, corpc_msg * msg)
{
  return corpc_channel_read(st, msg);
}




enum corpc_channel_state corpc_get_channel_state(const corpc_channel * channel)
{
  return channel->state;
}

void corpc_set_channel_state(corpc_channel * channel, enum corpc_channel_state state, int reason)
{
  channel->state = state;
  if ( channel->onstatechanged ) {
    channel->onstatechanged(channel, state, reason);
  }
}


bool corpc_channel_established(const corpc_channel * channel)
{
  return (channel->state == corpc_channel_state_connected || channel->state == corpc_channel_state_accepted);
}

static corpc_stream * find_stream_by_sid(const struct corpc_channel * channel, uint16_t sid)
{
  for ( size_t i = 0, n = ccarray_size(&channel->streams); i < n; ++i ) {
    corpc_stream * st = ccarray_ppeek(&channel->streams, i);
    if ( st->sid == sid ) {
      return st;
    }
  }
  return NULL;
}

static void on_create_stream_request(corpc_channel * channel, comsg ** msgp)
{
  (void)(channel);
  (void)(msgp);
}

static void on_create_stream_responce(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;
  // comsg_create_stream_responce * req = &(*msgp)->create_stream_responce;

  if ( !(st = find_stream_by_sid(channel, (*msgp)->hdr.did)) ) {
    // invalid stream
    CF_CRITICAL("find_stream_by_sid(did=%u) fails", (*msgp)->hdr.did);
  }
  else if ( !ccfifo_is_full(&st->rxq) ) {
    ccfifo_push(&st->rxq, msgp);
    *msgp = NULL;
    co_event_set(channel->stream_event);
  }
  else {
    // app or party bug
    CF_FATAL("ccfifo is full for sid=%u", st->sid);
    exit(1);
  }
}

static void on_close_stream_notify(corpc_channel * channel, comsg ** msgp)
{
  (void)(channel);
  (void)(msgp);
}

static void on_data_message(corpc_channel * channel, comsg ** msgp)
{
  (void)(channel);
  (void)(msgp);
}


static void corpc_channel_thread(void * arg)
{
  struct corpc_channel * channel = arg;
  co_socket * socket = NULL;
  co_ssl_socket * ssl_socket = NULL;

  comsg * msg = NULL;


  bool fok = true;

  switch ( channel->state ) {
    case corpc_channel_state_connecting : {
      ssl_socket = channel->ssl_sock;
      corpc_set_channel_state(channel, corpc_channel_state_connected, 0);
    }
    break;

    case corpc_channel_state_accepting : {
      socket = (co_socket*) channel->ssl_sock;
      if ( !(channel->ssl_sock = co_ssl_socket_accept(&socket, channel->listen_opts.ssl_ctx)) ) {
        break;
      }
      if ( channel->listen_opts.onaccepted && !(fok = channel->listen_opts.onaccepted(channel)) ) {
        break;
      }
      corpc_set_channel_state(channel, corpc_channel_state_accepted, 0);
    }
    break;

    default :
      CF_FATAL("App bug: invalid channel state %d", channel->state );
      exit(1);
    break;
  }

  if ( !fok ) {
    goto end;
  }


  msg = malloc(sizeof(*msg));

  while ( corpc_proto_recv_msg(ssl_socket, msg) ) {

    switch ( msg->hdr.code ) {

      case co_msg_create_stream_req :
        on_create_stream_request(channel, &msg);
      break;

      case co_msg_create_stream_resp :
        on_create_stream_responce(channel, &msg);
        break;

      case co_msg_close_stream_req :
        on_close_stream_notify(channel, &msg);
        break;

      case co_msg_data :
        on_data_message(channel, &msg);
        break;

      default :
        break;
    }

    if ( !msg ) {
      msg = malloc(sizeof(*msg));
    }
  }


end:

  free(msg);

  co_socket_close(&socket, false);
  co_ssl_socket_close(&ssl_socket, false);

}




void corpc_channel_cleanup(struct corpc_channel * channel)
{
  free(channel->connect_opts.connect_address), channel->connect_opts.connect_address = NULL;

  for ( size_t i = 0, n = ccarray_size(&channel->streams); i < n; ++i ) {
    corpc_stream_destroy(ccarray_peek(&channel->streams, i)); // fixme: this looks crazy, can not release chanell having active streams
  }

  ccarray_cleanup(&channel->streams);
  co_event_delete(&channel->stream_event);
  co_mutex_destroy(&channel->mtx);
}


bool corpc_channel_init(struct corpc_channel * channel, const struct corpc_channel_opts * opts)
{
  bool fok = false;

  channel->state = corpc_channel_state_idle;

  if ( !co_mutex_init(&channel->mtx) ) {
    goto end;
  }

  if ( !(channel->stream_event = co_event_create()) ) {
    goto end;
  }

  if ( !ccarray_init(&channel->streams, 256, sizeof(struct corpc_stream*)) ) {
    goto end;
  }

  if ( opts ) {
    if ( opts->connect_address && *opts->connect_address ) {
      if ( !(channel->connect_opts.connect_address = strdup(opts->connect_address)) ) {
        goto end;
      }
    }
    channel->connect_opts.connect_port = opts->connect_port;
    channel->connect_opts.connect_tmout_ms = opts->connect_tmout_ms;
    channel->connect_opts.ssl_ctx = opts->ssl_ctx;
    channel->onstatechanged = opts->onstatechanged;
  }

  fok  = true;

end:
  if ( !fok ) {
    corpc_channel_cleanup(channel);
  }

  return fok;
}


corpc_channel * corpc_channel_new(const corpc_channel_opts  * opts)
{
  corpc_channel * channel = NULL;
  bool fok = false;

  if ( !(channel = calloc(1, sizeof(*channel)))) {
    goto end;
  }

  if ( !corpc_channel_init(channel, opts) ) {
    goto end;
  }

  fok = true;

end:

  if ( !fok ) {
    corpc_channel_relase(&channel);
  }

  return channel;
}


void corpc_channel_relase(corpc_channel ** channel)
{
  if ( channel && *channel ) {
    corpc_channel_cleanup(*channel);
    free(*channel), *channel = NULL;
  }
}



void corpc_channel_set_client_context(corpc_channel * channel, void * client_context)
{
  channel->client_context = client_context;
}

void * corpc_channel_get_client_context(const corpc_channel * channel)
{
  return channel->client_context;
}


bool corpc_channel_open_internal(corpc_channel * channel, bool lock)
{

  bool fok = false;

  if ( lock ) {
   co_mutex_lock(&channel->mtx);
  }

  if ( channel->state == corpc_channel_state_connected ) {
    fok = true;
    goto end;
  }

  if ( channel->state != corpc_channel_state_idle ) {
    errno = EINVAL;
    goto end;
  }

  corpc_set_channel_state(channel, corpc_channel_state_connecting, 0);

  channel->ssl_sock = co_ssl_server_connect(channel->connect_opts.connect_address, channel->connect_opts.connect_port,
      &(struct co_ssl_connect_opts ) {
            .ssl_ctx = channel->connect_opts.ssl_ctx,
            .sock_type = SOCK_STREAM,
            .proto = IPPROTO_TCP,
            .tmout = channel->connect_opts.connect_tmout_ms,
          });

  if ( channel->ssl_sock ) {
    fok = co_schedule(corpc_channel_thread, channel, CORPC_CHANNEL_THREAD_STACK_SIZE);
  }

  if ( !fok ) {
    corpc_set_channel_state(channel, corpc_channel_state_disconnecting, errno);
    co_ssl_socket_close(&channel->ssl_sock, true);
    corpc_set_channel_state(channel, corpc_channel_state_idle, errno);
  }

end:

  if ( lock ) {
    co_mutex_unlock(&channel->mtx);
  }

  return fok;
}

bool corpc_open_channel(corpc_channel * channel)
{
  return corpc_channel_open_internal(channel, true);
}


corpc_channel * corpc_channel_accept(corpc_listening_port * clp, const co_socket * accepted_sock)
{
  corpc_channel * channel = NULL;

  bool fok = false;

  if ( !(channel = corpc_channel_new(NULL)) ) {
    goto end;
  }

  channel->state = corpc_channel_state_accepting;
  channel->ssl_sock = (co_ssl_socket * )accepted_sock; // hack
  channel->listen_opts.onaccepted = clp->onaccepted;
  channel->listen_opts.ondisconnected = clp->ondisconnected;
  channel->listen_opts.services = clp->services;
  channel->listen_opts.nb_services = clp->nb_services;
  channel->listen_opts.ssl_ctx = clp->base.ssl_ctx;

  if ( !co_schedule(corpc_channel_thread, channel, CORPC_CHANNEL_THREAD_STACK_SIZE) ) {
    channel->state = corpc_channel_state_idle;
    goto end;
  }

  fok = true;

end:

  if ( !fok ) {
    corpc_channel_relase(&channel);
  }

  return channel;
}



void corpc_close_channel(corpc_channel * channel)
{
  (void)(channel);
}




static bool corpc_channel_request_open_stream(corpc_channel * channel, corpc_stream * st, const char * service, const char * method)
{
  (void)(service);
  (void)(method);

  co_event_waiter * w = NULL;
  comsg * msg = NULL;

  bool fok = false;

  if ( !(w = co_event_add_waiter(channel->stream_event)) ) {
    goto end;
  }

  if ( !corpc_proto_send_create_stream_request(st->channel->ssl_sock, st->sid) ) {
    goto end;
  }

  while ( corpc_channel_established(channel) && ccfifo_is_empty(&st->rxq) ) {
    co_event_wait(w, -1);
  }

  if ( !(msg = ccfifo_ppop(&st->rxq)) ) {
    CF_ERROR("ccfifo_ppop() st=%d fails", st->sid);
    goto end;
  }

  if ( msg->hdr.code != co_msg_create_stream_resp ) {
    CF_ERROR("invalid message code %u when expected co_msg_create_stream_resp=%u st=%d", msg->hdr.code,
        co_msg_create_stream_resp, st->sid);
    goto end;
  }

  if ( msg->create_stream_responce.details.status != 0 ) {
    CF_ERROR("remote party refused creating new stream. responce status=%u",
        msg->create_stream_responce.details.status);
    goto end;
  }

  st->did = msg->hdr.sid;
  st->state = corpc_stream_state_opened;
  fok = true;

end:

  co_event_remove_waiter(channel->stream_event, w);

  return fok;
}


corpc_stream * corpc_open_stream(corpc_channel * channel, const corpc_open_stream_opts * opts)
{
  corpc_stream * st = NULL;
  bool fok = false;

  static uint16_t ssid = 1;

  co_mutex_lock(&channel->mtx);

  if ( ccarray_size(&channel->streams) >= ccarray_capacity(&channel->streams) ) {
    errno = ENOSR;
    goto end;
  }

  st = corpc_stream_new(&(corpc_stream_opts ) {
        .channel = channel,
        .state = corpc_stream_state_opening,
        .sid = ssid++,
        .did = 0,
      });

  if ( !st ) {
    goto end;
  }

  ccarray_ppush_back(&channel->streams, st);

  // make sure connection is established
  if ( !corpc_channel_open_internal(channel, false) ) {
    goto end;
  }

  co_mutex_unlock(&channel->mtx);
  fok = corpc_channel_request_open_stream(channel, st, opts->service, opts->method);
  co_mutex_lock(&channel->mtx);

end:

  if ( !fok ) {
    corpc_stream_destroy(&st);
  }

  co_mutex_unlock(&channel->mtx);

  return st;
}


void corpc_close_stream(corpc_stream * stp)
{
//  if ( stp && *stp) {
//    ccarray_erase_item(&(*stp)->channel->streams, stp);
//    corpc_stream_release(stp);
//  }
}




