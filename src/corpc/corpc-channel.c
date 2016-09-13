/*
 * corpc-channel.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */


#include <cuttle/debug.h>
#include <cuttle/ssl/error.h>
#include "corpc-channel.h"
#include "corpc-listening-port.h"
#include "corpc-proto.h"
#include <errno.h>

#define CORPC_CHANNEL_THREAD_STACK_SIZE   (1024*1024)
#define CORPC_STREAM_DEFAULT_QUEUE_SIZE   8

#define CORPC_STREAM_DEFAULT_STACK_SIZE   (2*1024*1024)




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
  }
  return "bug:invalid-channel-state";
}

const char * corpc_stream_state_string(enum corpc_stream_state state)
{
  switch ( state ) {
    case corpc_stream_created :
      return "created";
    case corpc_stream_opening :
      return "opening";
    case corpc_stream_too_many_streams:
      return "closed:too_many_streams";
    case corpc_stream_no_such_service:
      return "closed:no_such_service";
    case corpc_stream_no_such_method:
      return "closed:no_such_method";
    case corpc_stream_local_internal_error:
      return "closed:local_internal_error";
    case corpc_stream_remote_internal_error:
      return "closed:remote_internal_error";
    case corpc_stream_protocol_error:
      return "closed:protocol_error";
    case corpc_stream_established :
      return "established";
    case corpc_stream_closed :
      return "closed";
    case corpc_stream_closed_by_remote_party :
      return "closed:by_remote_party";
  }
  return "bug:invalid-stream-state";
}


static uint16_t gensid(void)
{
  static uint16_t sid = 1;
  return ++sid;
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
  corpc_stream_state oldstate = st->state;
  CF_NOTICE("%s -> %s", corpc_stream_state_string(oldstate), corpc_stream_state_string(state));

  st->state = state;
  if ( st->channel ) {
    co_event_set(st->channel->stream_event);
  }
}

void * corpc_stream_get_channel_client_context(const corpc_stream * stream)
{
  return corpc_channel_get_client_context(stream->channel);
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
  comsg_create_stream_request * rc = &(*msgp)->create_stream_request;

  const size_t service_name_length = rc->details.service_name_length;
  const size_t method_name_length = rc->details.method_name_length;

  char service_name[service_name_length + 1];
  char method_name[method_name_length + 1];

  const struct corpc_service * service = NULL;
  const struct corpc_service_method * method = NULL;
  corpc_stream * st = NULL;

  uint16_t did = (*msgp)->hdr.sid;
  uint16_t sid = 0;

  create_stream_responce_code status =
        create_stream_responce_internal_error;


  co_mutex_lock(&channel->mtx);

  if ( ccarray_size(&channel->streams) >= ccarray_capacity(&channel->streams) ) {
    CF_CRITICAL("Too many streams");
    status = create_stream_responce_no_stream_resources;
    goto end;
  }

  memcpy(service_name, rc->details.pack + 0, service_name_length);
  memcpy(method_name, rc->details.pack + service_name_length, method_name_length);

  service_name[service_name_length] = 0;
  method_name[method_name_length] = 0;


  for ( int i = 0; i < channel->listen_opts.nb_services; ++i ) {
    if ( strcmp(service_name, channel->listen_opts.services[i]->name) == 0 ) {
      service = channel->listen_opts.services[i];
      break;
    }
  }

  if ( !service ) {
    CF_CRITICAL("Requested service '%s' not found", service_name);
    status = create_stream_responce_no_service;
    goto end;
  }


  for ( int i = 0; service->methods[i].name != NULL; ++i ) {
    if ( strcmp(method_name, service->methods[i].name) == 0 ) {
      method = &service->methods[i];
      break;
    }
  }

  if ( !method ) {
    CF_CRITICAL("Requested method '%s' on service '%s' not found", method_name, service_name);
    status = create_stream_responce_no_method;
    goto end;
  }


  st = corpc_stream_new(&(struct corpc_stream_opts ) {
        .channel = channel,
        .state = corpc_stream_opening,
        .sid = gensid(),
        .did = did
      });

  if ( !st ) {
    CF_CRITICAL("corpc_stream_new() fails");
    status = create_stream_responce_internal_error;
    goto end;
  }

  ccarray_ppush_back(&channel->streams, st);


  // fixme: void * pointer required, create a sub routine ?
  if ( !co_schedule((void (*)(void*))method->proc, st, CORPC_STREAM_DEFAULT_STACK_SIZE) ) {
    CF_CRITICAL("co_schedule() fails");
    status = create_stream_responce_internal_error;
    goto end;
  }

  sid = st->sid;
  status = create_stream_responce_ok;

end:

  if ( st ) {
    if ( status != create_stream_responce_ok ) {
      ccarray_erase_item(&channel->streams, &st);
    }
    else {
      corpc_set_stream_state(st, corpc_stream_established);
    }
  }

  corpc_proto_send_create_stream_responce(channel->ssl_sock, sid, did, status);

  co_mutex_unlock(&channel->mtx);
}


static void on_create_stream_responce(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;
  corpc_stream_state state;

  uint16_t sid = (*msgp)->hdr.did;
  uint16_t did = (*msgp)->hdr.sid;

  co_mutex_lock(&channel->mtx);

  if ( !(st = find_stream_by_sid(channel, sid)) ) {
    CF_CRITICAL("find_stream_by_sid(sid=%u) fails", sid);
  }
  else if ( st->state != corpc_stream_opening ) {
    CF_CRITICAL("BUG: Invalid stream state : sid=%u state=%s", sid, corpc_stream_state_string(st->state));
  }
  else {

    switch ( (*msgp)->create_stream_responce.details.status ) {
      case create_stream_responce_ok :
        state = corpc_stream_established;
        st->did = did;
      break;
      case create_stream_responce_no_stream_resources :
        state = corpc_stream_too_many_streams;
      break;
      case create_stream_responce_no_service :
        state = corpc_stream_no_such_service;
      break;
      case create_stream_responce_no_method :
        state = corpc_stream_no_such_method;
      break;
      case create_stream_responce_internal_error :
        state = corpc_stream_remote_internal_error;
      break;
      case create_stream_responce_protocol_error :
        default :
        state = corpc_stream_protocol_error;
      break;
    }

    if ( state != corpc_stream_established ) {
      CF_CRITICAL("Create stream %u fails: %s", sid, corpc_stream_state_string(state));
    }

    corpc_set_stream_state(st, state);
  }

  co_mutex_unlock(&channel->mtx);

}

static void on_close_stream_notify(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;

  co_mutex_lock(&channel->mtx);

  if ( !(st = find_stream_by_sid(channel, (*msgp)->hdr.did)) ) {
    CF_CRITICAL("find_stream_by_sid(did=%u) fails", (*msgp)->hdr.did);

    CF_CRITICAL("<STREAM LIST>:");

    for ( size_t i = 0, n = ccarray_size(&channel->streams); i < n; ++i ) {
      corpc_stream * tmp = ccarray_ppeek(&channel->streams, i);
      CF_CRITICAL("  stream[%zu] : sid=%u did=%u %s", i, tmp->sid, tmp->did, corpc_stream_state_string(tmp->state));
    }

    CF_CRITICAL("</STREAM LIST>:");
  }
  else {
    corpc_set_stream_state(st, corpc_stream_closed_by_remote_party);
  }

  co_mutex_unlock(&channel->mtx);
}

static void on_data_message(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;

  co_mutex_lock(&channel->mtx);

  if ( !(st = find_stream_by_sid(channel, (*msgp)->hdr.did)) ) {
    CF_CRITICAL("find_stream_by_sid(did=%u) fails", (*msgp)->hdr.did);

    CF_CRITICAL("<STREAM LIST>:");

    for ( size_t i = 0, n = ccarray_size(&channel->streams); i < n; ++i ) {
      corpc_stream * tmp = ccarray_ppeek(&channel->streams, i);
      CF_CRITICAL("  stream[%zu] : sid=%u did=%u %s", i, tmp->sid, tmp->did, corpc_stream_state_string(tmp->state));
    }

    CF_CRITICAL("</STREAM LIST>:");

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

  co_mutex_unlock(&channel->mtx);
}


static void corpc_channel_thread(void * arg)
{
  struct corpc_channel * channel = arg;
  co_socket * socket = NULL;

  comsg * msg = NULL;

  CF_INFO("ENTER channel=%p state=%s", channel, corpc_channel_state_string(channel->state));

  bool fok = true;

  switch ( channel->state ) {
    case corpc_channel_state_connecting : {
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

  while ( corpc_proto_recv_msg(channel->ssl_sock, msg) ) {


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
        CF_CRITICAL("Unknown message code received: %u", msg->hdr.code);
        break;
    }


    if ( !msg ) {
      msg = malloc(sizeof(*msg));
    }
  }

end:

  free(msg);

  co_socket_close(&socket, false);
  co_ssl_socket_close(&channel->ssl_sock, false);

  CF_INFO("FINIDHED channel=%p", channel);

}




void corpc_channel_cleanup(struct corpc_channel * channel)
{
  free(channel->connect_opts.connect_address), channel->connect_opts.connect_address = NULL;

  CF_NOTICE("NB_STREAMS=%zu", ccarray_size(&channel->streams));

  for ( size_t i = 0, n = ccarray_size(&channel->streams); i < n; ++i ) {
    CF_NOTICE("C corpc_stream_destroy(%zu)", i);
    corpc_stream_destroy(ccarray_peek(&channel->streams, i)); // fixme: this looks crazy, can not release chanell having active streams
    CF_NOTICE("R corpc_stream_destroy(%zu)", i);
  }

  CF_NOTICE("C ccarray_cleanup(&channel->streams)");
  ccarray_cleanup(&channel->streams);
  CF_NOTICE("C co_event_delete(&channel->stream_event)");
  co_event_delete(&channel->stream_event);
  CF_NOTICE("C co_mutex_destroy(&channel->mtx)");
  co_mutex_destroy(&channel->mtx);
  CF_NOTICE("LEAVE");
}


bool corpc_channel_init(struct corpc_channel * channel, const struct corpc_channel_opts * opts)
{
  bool fok = false;

  channel->state = corpc_channel_state_idle;

  if ( !co_mutex_init(&channel->mtx) ) {
    CF_SSL_ERR(CF_SSL_ERR_APP, "co_mutex_init() fails: %s", strerror(errno));
    goto end;
  }

  if ( !(channel->stream_event = co_event_create()) ) {
    CF_SSL_ERR(CF_SSL_ERR_APP, "co_event_create() fails: %s", strerror(errno));
    goto end;
  }

  if ( !ccarray_init(&channel->streams, 256, sizeof(struct corpc_stream*)) ) {
    CF_SSL_ERR(CF_SSL_ERR_APP, "ccarray_init(streams) fails: %s", strerror(errno));
    goto end;
  }

  if ( opts ) {
    if ( opts->connect_address && *opts->connect_address ) {
      if ( !(channel->connect_opts.connect_address = strdup(opts->connect_address)) ) {
        CF_SSL_ERR(CF_SSL_ERR_APP, "strdup(connect_address) fails: %s", strerror(errno));
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
    CF_SSL_ERR(CF_SSL_ERR_MALLOC,"calloc(channel) fails: %s", strerror(errno));
    goto end;
  }

  if ( !corpc_channel_init(channel, opts) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC,"corpc_channel_init() fails: %s", strerror(errno));
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
    CF_DEBUG("C corpc_channel_cleanup(*channel)");
    corpc_channel_cleanup(*channel);
    CF_DEBUG("C free(*channel)");
    free(*channel), *channel = NULL;
    CF_DEBUG("R free(*channel)");
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
    CF_CRITICAL("co_schedule(corpc_channel_thread) fails: %s", strerror(errno));
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




static bool corpc_request_open_stream(corpc_channel * channel, corpc_stream * st, const char * service, const char * method)
{
  co_event_waiter * w = NULL;

  bool fok = false;

  if ( !(w = co_event_add_waiter(channel->stream_event)) ) {
    CF_CRITICAL("co_event_add_waiter() fails:%s", strerror(errno) );
    goto end;
  }

  if ( !corpc_proto_send_create_stream_request(st->channel->ssl_sock, st->sid, service, method) ) {
    CF_CRITICAL("corpc_proto_send_create_stream_request() fails");
    goto end;
  }

  while ( st->state == corpc_stream_opening ) {
    co_event_wait(w, -1);
  }

  if ( st->state != corpc_stream_established ) {
    CF_CRITICAL("NOT ESTABLISHED");
    goto end;
  }

  fok = true;

end:

  co_event_remove_waiter(channel->stream_event, w);

  return fok;
}


corpc_stream * corpc_open_stream(corpc_channel * channel, const corpc_open_stream_opts * opts)
{
  corpc_stream * st = NULL;
  bool fok = false;


  co_mutex_lock(&channel->mtx);

  if ( ccarray_size(&channel->streams) >= ccarray_capacity(&channel->streams) ) {
    CF_CRITICAL("NO STREAM RESOURCES");
    errno = ENOSR;
    goto end;
  }

  st = corpc_stream_new(&(corpc_stream_opts ) {
        .channel = channel,
        .state = corpc_stream_opening,
        .sid = gensid(),
        .did = 0,
      });

  if ( !st ) {
    CF_CRITICAL("corpc_stream_new() fails");
    goto end;
  }

  ccarray_ppush_back(&channel->streams, st);

  // make sure connection is established
  if ( !corpc_channel_open_internal(channel, false) ) {
    CF_CRITICAL("corpc_channel_open_internal() fails");
    goto end;
  }

  co_mutex_unlock(&channel->mtx);
  fok = corpc_request_open_stream(channel, st, opts->service, opts->method);
  co_mutex_lock(&channel->mtx);

  if( !fok ) {
    CF_CRITICAL("corpc_request_open_stream() fails");
  }



end:

  if ( !fok && st ) {
    ccarray_erase_item(&channel->streams, &st);
    corpc_stream_destroy(&st);
  }

  co_mutex_unlock(&channel->mtx);

  return st;
}

void corpc_close_stream(corpc_stream ** stp )
{
  if ( stp && *stp ) {

    corpc_stream * st = *stp;
    corpc_channel * channel = st->channel;

    if ( !corpc_proto_send_close_stream(st->channel->ssl_sock, st->sid, st->did, 0) ) {
      CF_CRITICAL("corpc_proto_send_close_stream() fails");
    }

    co_mutex_lock(&channel->mtx);
    ccarray_erase_item(&channel->streams, stp);
    co_mutex_unlock(&channel->mtx);

    corpc_stream_destroy(stp);
  }
}

bool corpc_channel_read(struct corpc_stream * st, corpc_msg * ccmsg)
{
  corpc_channel * channel = st->channel;
  co_event_waiter * w = NULL;
  struct comsg * comsg = NULL;

  bool fok = false;

  // CF_DEBUG("ENTER");


  if ( !(w = co_event_add_waiter(channel->stream_event)) ) {
    CF_CRITICAL("co_event_add_waiter() fails");
    goto end;
  }

  // fixme: co_event really required a co_mutex due to multi-core scheduling
  CF_DEBUG("Start wait for data message st->state=%s", corpc_stream_state_string(st->state));

  while ( ccfifo_is_empty(&st->rxq) && (st->state == corpc_stream_established || st->state == corpc_stream_opening) ) {
    co_event_wait(w, -1);
  }

  if ( !(comsg = ccfifo_ppop(&st->rxq)) ) {
    CF_ERROR("ccfifo_ppop() st=%d fails. st->state=%s", st->sid, corpc_stream_state_string(st->state) );
    goto end;
  }

  if ( comsg->hdr.code != co_msg_data ) {
    CF_CRITICAL("invalid message code %u when expected co_msg_data=%u st=%d", comsg->hdr.code,
        co_msg_data, st->sid);
    goto end;
  }

  if ( !(ccmsg->data = malloc(comsg->data.hdr.pldsize)) ) {
    CF_CRITICAL("malloc(ccmsg->data) fails: %s", strerror(errno));
    goto end;
  }

  memcpy(ccmsg->data, comsg->data.details.bits, ccmsg->size = comsg->data.hdr.pldsize);

  fok = true;

end:

  co_event_remove_waiter(channel->stream_event, w);

  free(comsg);

  //CF_DEBUG("LEAVE: fok=%d", fok);

  return fok;
}

bool corpc_channel_write(struct corpc_stream * st, const corpc_msg * msg)
{
  bool fok;

  co_mutex_lock(&st->channel->mtx);
  fok = corpc_proto_send_data(st->channel->ssl_sock, st->sid, st->did, msg->data, msg->size);
  co_mutex_unlock(&st->channel->mtx);

  return fok;
}



