/*
 * corpc-channel.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */


#include <cuttle/debug.h>
#include <cuttle/time.h>
#include <cuttle/ssl/error.h>
#include <cuttle/cothread/resolve.h>
#include "corpc-channel.h"
#include "corpc-listening-port.h"
#include "corpc-proto.h"
#include <errno.h>

#define CORPC_CHANNEL_THREAD_STACK_SIZE   (1024*1024)
#define CORPC_STREAM_DEFAULT_QUEUE_SIZE   8

#define CORPC_STREAM_DEFAULT_STACK_SIZE   (2*1024*1024)


/**
 * client channel life time
 *
 *   1) channel = corpc_channel_new()
 *           state => idle
 *
 *   2) corpc_channel_open()
 *           state => connecting
 *              ssl_sock = co_ssl_socket * co_ssl_connect()
 *              corpc_channel_thread()
 *                   state => connected
 *           leave
 *
 *   3)
 *
 *
 */


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
    case corpc_channel_state_closed:
      return "closed";
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



static co_thread_lock_t * g_global_lock =
      CO_THREAD_LOCK_INITIALIZER;

static void channel_state_lock(void)
{
  if ( !co_thread_lock(&g_global_lock) ) {
    CF_FATAL("co_thread_lock() fails: %s", strerror(errno));
  }
}

static void channel_state_unlock(void)
{
  if ( !co_thread_unlock(&g_global_lock) ) {
    CF_FATAL("co_thread_unlock() fails: %s", strerror(errno));
  }
}

static void channel_state_wait(int tmo)
{
  if ( co_thread_wait(&g_global_lock, tmo) < 0 ) {
    CF_FATAL("co_thread_wait() fails: %s", strerror(errno));
  }
}

static void channel_state_signal(void)
{
  if ( co_thread_signal(&g_global_lock) < 0 ) {
    CF_FATAL("co_thread_broadcast() fails: %s", strerror(errno));
  }
}

static bool acquire_write_lock(corpc_channel * channel, int tmo, bool * track)
{
  int64_t ct, et = tmo > 0 ? cf_get_monotic_ms() + tmo : -1;

  *track = false;

  channel_state_lock();

  while ( channel->write_lock && corpc_channel_established(channel) && (tmo < 0 || (ct = cf_get_monotic_ms()) < et) ) {
    channel_state_wait(tmo < 0 ? -1 : (int) (et - ct));
  }

  if ( channel->write_lock ) {
    errno = ETIME;
  }
  else if ( !corpc_channel_established(channel) ) {
    errno = ENOTCONN;
  }
  else {
    *track = channel->write_lock = true;
  }

  channel_state_unlock();

  return *track;
}

static void release_write_lock(corpc_channel * channel, bool * track)
{
  if ( *track ) {

    channel_state_lock();

    *track = channel->write_lock = false;
    channel_state_signal();

    channel_state_unlock();
  }
}


///////////////////////////////////////////////////////////////////////////////////


// must be locked
void set_channel_state(corpc_channel * channel, enum corpc_channel_state state, int reason)
{
  CF_NOTICE("%s -> %s : %s", corpc_channel_state_string(channel->state), corpc_channel_state_string(state),
      strerror(reason));

  channel->state = state;
  channel_state_signal();

  if ( channel->onstatechanged ) {
    channel->onstatechanged(channel, state, reason);
  }
}



// channel must be locked
static uint16_t gensid(void)
{
  static uint16_t gsid = 0;
  uint16_t sid;

  //lock_channel();
  sid = ++gsid;
  //unlock_channel();

  return sid;
}


static bool send_create_stream_request(corpc_stream * st, const char * service, const char * method)
{
  bool write_lock = false;
  bool fok = false;

  if ( !acquire_write_lock(st->channel, -1, &write_lock) ) {
    CF_ERROR("acquire_write_lock() fails");
  }
  else if ( !(fok = corpc_proto_send_create_stream_request(st->channel->ssl_sock, st->sid, service, method)) ) {
    CF_ERROR("corpc_proto_send_create_stream_request() fails");
  }

  release_write_lock(st->channel, &write_lock);

  return fok;
}

static bool send_create_stream_responce(corpc_channel * channel, uint16_t sid, uint16_t did, uint16_t status)
{
  bool write_lock = false;
  bool fok = false;

  if ( !acquire_write_lock(channel, -1, &write_lock) ) {
    CF_ERROR("acquire_write_lock() fails");
  }
  else if ( !(fok = corpc_proto_send_create_stream_responce(channel->ssl_sock, sid, did, status))) {
    CF_ERROR("corpc_proto_send_create_stream_responce() fails");
  }

  release_write_lock(channel, &write_lock);

  return fok;
}

static bool send_close_stream_notify(corpc_stream * st)
{
  bool write_lock = false;
  bool fok = false;

  if ( !acquire_write_lock(st->channel, -1, &write_lock) ) {
    CF_ERROR("acquire_write_lock() fails");
  }
  else if ( !(fok = corpc_proto_send_close_stream(st->channel->ssl_sock, st->sid, st->did, 0)) ) {
    CF_ERROR("corpc_proto_send_close_stream() fails");
  }

  release_write_lock(st->channel, &write_lock);

  return fok;
}

static ssize_t send_data(corpc_stream * st, const void * data, size_t size)
{
  bool write_lock = false;
  ssize_t bytes_sent = -1;

  if ( !acquire_write_lock(st->channel, -1, &write_lock) ) {
    CF_ERROR("acquire_write_lock() fails");
  }
  else if ( !corpc_proto_send_data(st->channel->ssl_sock, st->sid, st->did, data, size) ) {
    CF_ERROR("corpc_proto_send_data() fails");
  }
  else {
    bytes_sent = size;
  }

  release_write_lock(st->channel, &write_lock);

  return bytes_sent;
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
  channel_state_signal();
}

void * corpc_stream_get_channel_client_context(const corpc_stream * stream)
{
  return corpc_channel_get_client_context(stream->channel);
}





enum corpc_channel_state corpc_get_channel_state(const corpc_channel * channel)
{
  return channel->state;
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

// on locked channel

static corpc_stream * accept_stream(corpc_channel * channel, uint16_t did, create_stream_responce_code * status)
{
  corpc_stream * st = NULL;

  channel_state_lock();

  if ( ccarray_size(&channel->streams) >= ccarray_capacity(&channel->streams) ) {
    CF_CRITICAL("Too many streams");
    *status = create_stream_responce_no_stream_resources;
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
    *status = create_stream_responce_internal_error;
    goto end;
  }

  ccarray_ppush_back(&channel->streams, st);

end :

  channel_state_unlock();

  return st;
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

  if ( !(st = accept_stream(channel, did, &status)) ) {
    CF_CRITICAL("accept_stream(did=%u) fails", did);
    goto end;
  }

  // fixme: void * pointer required, create a sub routine ?
  if ( !co_schedule((void (*)(void*))method->proc, st, CORPC_STREAM_DEFAULT_STACK_SIZE) ) {
    CF_CRITICAL("co_schedule(method->proc) fails");
    status = create_stream_responce_internal_error;
    goto end;
  }

  sid = st->sid;
  status = create_stream_responce_ok;

end:

  if ( !send_create_stream_responce(channel, sid, did, status) ) {
    CF_CRITICAL("send_create_stream_responce() fails");
  }

  if ( st ) {

    channel_state_lock();

    if ( status == create_stream_responce_ok ) {
      corpc_set_stream_state(st, corpc_stream_established);
    }
    else {
      ccarray_erase_item(&channel->streams, &st);
      corpc_stream_destroy(&st);
    }

    channel_state_unlock();
  }
}


static void on_create_stream_responce(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;
  corpc_stream_state state;

  uint16_t sid = (*msgp)->hdr.did;
  uint16_t did = (*msgp)->hdr.sid;


  CF_DEBUG("enter");

  channel_state_lock();

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

  channel_state_unlock();

  CF_DEBUG("leave");
}

static void on_close_stream_notify(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;

  channel_state_lock();

  if ( (st = find_stream_by_sid(channel, (*msgp)->hdr.did)) ) {
    corpc_set_stream_state(st, corpc_stream_closed_by_remote_party);
  }
  else {
    CF_CRITICAL("find_stream_by_sid(did=%u) fails", (*msgp)->hdr.did);

    CF_CRITICAL("<STREAM LIST>:");

    for ( size_t i = 0, n = ccarray_size(&channel->streams); i < n; ++i ) {
      corpc_stream * tmp = ccarray_ppeek(&channel->streams, i);
      CF_CRITICAL("  stream[%zu] : sid=%u did=%u %s", i, tmp->sid, tmp->did, corpc_stream_state_string(tmp->state));
    }

    CF_CRITICAL("</STREAM LIST>:");
  }

  channel_state_unlock();
}

static void on_data_message(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;

  channel_state_lock();

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
    channel_state_signal();
  }
  else {
    // app or party bug
    CF_FATAL("ccfifo is full for sid=%u", st->sid);
    exit(1);
  }

  channel_state_unlock();
}



static void corpc_channel_thread(void * arg)
{
  struct corpc_channel * channel = arg;
  comsg * msg = NULL;

  bool fok = true;

  channel_state_lock();

  if ( channel->state == corpc_channel_state_connecting ) {
    // actual ssl handshake already made in corpc_channel_open()
    set_channel_state(channel, corpc_channel_state_connected, 0);
  }

  else if ( channel->state == corpc_channel_state_accepting ) {

    channel_state_unlock();
    fok = co_ssl_socket_accept(channel->ssl_sock);
    channel_state_lock();

    if ( fok ) {
      set_channel_state(channel, corpc_channel_state_accepted, 0);
    }
    else {
      CF_CRITICAL("co_ssl_socket_accept() fails");
    }
  }
  else {
    CF_FATAL("Unexpected channel state %s", corpc_channel_state_string(channel->state));
  }

  if ( !fok ) {
    goto end;
  }

  channel_state_unlock();

  if ( !(msg = malloc(sizeof(*msg))) ) {
    CF_FATAL("malloc(msg) fails");
  }

 // CF_WARNING("channel->ssl_sock=%p", channel->ssl_sock);
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

    if ( !msg && !(msg = malloc(sizeof(*msg))) ) {
      CF_FATAL("malloc(msg) fails");
    }
  }

  channel_state_lock();

  end :

  co_ssl_socket_close(&channel->ssl_sock, false);
  set_channel_state(channel, corpc_channel_state_closed, errno);
  channel_state_unlock();

  free(msg);

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

  CF_NOTICE("LEAVE");
}


bool corpc_channel_init(struct corpc_channel * channel, const struct corpc_channel_opts * opts)
{
  bool fok = false;

  channel->state = corpc_channel_state_idle;
  channel->refs = 0;

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



static void channel_destroy(corpc_channel ** channel)
{
  if ( channel && *channel ) {
    corpc_channel_cleanup(*channel);
    free(*channel);
    *channel = NULL;
  }
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

  channel->refs = 1;
  fok = true;

end:

  if ( !fok ) {
    channel_destroy(&channel);
  }

  return channel;
}


//void corpc_channel_addref(corpc_channel * channel)
//{
//  global_lock();
//  ++channel->refs;
//  global_unlock();
//}

void corpc_channel_relase(corpc_channel ** channel)
{
  if ( channel && *channel ) {

    int refs;

    //global_lock();
    refs = --(*channel)->refs;
    //global_unlock();

    if ( refs > 0 ) {
      *channel = NULL;
    }
    else {
      corpc_channel_close(*channel);
      channel_destroy(channel);
    }
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


static bool start_channel_thread(corpc_channel * channel, enum corpc_channel_state initial_state)
{
  bool fok = false;

  channel->state = initial_state;

  if ( !co_schedule(corpc_channel_thread, channel, CORPC_CHANNEL_THREAD_STACK_SIZE) ) {
    set_channel_state(channel, corpc_channel_state_idle, errno);
    CF_CRITICAL("co_schedule(corpc_channel_thread) fails: %s", strerror(errno));
  }
  else {
    while ( channel->state == initial_state ) {
      channel_state_wait(-1);
    }
    if ( !(fok = corpc_channel_established(channel)) ) {
      CF_CRITICAL("NOT ESTABLISHED: %s", corpc_channel_state_string(channel->state));
    }
  }

  return fok;
}

static bool ssl_server_connect(corpc_channel * channel)
{
  struct addrinfo * ai = NULL;
  co_ssl_socket * ssl_sock = NULL;
  bool fok = false;

  channel_state_unlock();

  if ( !co_server_resolve(&ai, channel->connect_opts.connect_address, channel->connect_opts.connect_port,
      channel->connect_opts.connect_tmout_ms) ) {
    CF_CRITICAL("co_server_resolve() fails");
  }
  else if ( !(ssl_sock = co_ssl_socket_new(ai->ai_family, SOCK_STREAM, IPPROTO_TCP, channel->connect_opts.ssl_ctx)) ) {
    CF_CRITICAL("co_ssl_socket_new() fails");
  }
  else {

    channel_state_lock();
    channel->ssl_sock = ssl_sock;
    channel_state_unlock();

    if ( !co_ssl_connect(channel->ssl_sock, ai->ai_addr, channel->connect_opts.connect_tmout_ms) ) {
      CF_CRITICAL("co_ssl_connect() fails");
    }
    else {
      fok = true;
    }
  }

  if ( ai ) {
    freeaddrinfo(ai);
  }

  channel_state_lock();

  if ( !fok ) {
    co_ssl_socket_close(&channel->ssl_sock, true);
  }

  return (channel->ssl_sock != NULL);
}

bool corpc_channel_open(corpc_channel * channel)
{
  bool fok = false;

  channel_state_lock();

  if ( channel->state == corpc_channel_state_connected ) {
    fok = true;
  }
  else if ( channel->state != corpc_channel_state_idle ) {
    errno = EINVAL;
  }
  else if ( !ssl_server_connect(channel) ) {
    CF_CRITICAL("create_channel_socket() fails");
  }
  else if ( !(fok = start_channel_thread(channel, corpc_channel_state_connecting)) ) {
    CF_CRITICAL("start_channel_thread() fails");
    co_ssl_socket_close(&channel->ssl_sock, false);
  }

  channel_state_unlock();

  return fok;
}


corpc_channel * corpc_channel_accept(corpc_listening_port * clp, co_ssl_socket * accepted_sock)
{
  corpc_channel * channel = NULL;

  if ( !(channel = corpc_channel_new(NULL)) ) {
    CF_CRITICAL("corpc_channel_new() fails: %s", strerror(errno));
  }
  else {

    channel->ssl_sock = accepted_sock;
    channel->listen_opts.onaccepted = clp->onaccepted;
    channel->listen_opts.ondisconnected = clp->ondisconnected;
    channel->listen_opts.services = clp->services;
    channel->listen_opts.nb_services = clp->nb_services;
    channel->listen_opts.ssl_ctx = clp->base.ssl_ctx;

    channel_state_lock();

    if ( !start_channel_thread(channel, corpc_channel_state_accepting) ) {
      CF_CRITICAL("start_channel_thread() fails");
      channel_destroy(&channel);
    }

    channel_state_unlock();
  }

  return channel;
}



void corpc_channel_close(corpc_channel * channel)
{
  switch (channel->state) {
    case corpc_channel_state_idle:
      break;
    case corpc_channel_state_connecting:
      break;
    case corpc_channel_state_connected:
      break;
    case corpc_channel_state_accepting:
      break;
    case corpc_channel_state_accepted:
      break;
    case corpc_channel_state_disconnecting:
      break;
    case corpc_channel_state_closed:
      break;
  }

}




static corpc_stream * create_new_stream(corpc_channel * channel)
{
  corpc_stream * st = NULL;

  channel_state_lock();

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

end:
  channel_state_unlock();

  return st;
}

static void destroy_stream(corpc_stream ** st)
{
  if ( st && *st ) {
    channel_state_lock();
    if ( (*st)->channel ) {
      ccarray_erase_item(&(*st)->channel->streams, st);
    }
    corpc_stream_destroy(st);
    channel_state_unlock();
  }
}

corpc_stream * corpc_open_stream(corpc_channel * channel, const corpc_open_stream_opts * opts)
{
  corpc_stream * st = NULL;

  bool fok = false;

  if ( !corpc_channel_open(channel) ) {    // check if connection is established
    CF_CRITICAL("corpc_channel_open() fails");
  }
  else if ( !(st = create_new_stream(channel)) ) {
    CF_CRITICAL("create_new_stream() fails");
  }
  else if ( !send_create_stream_request(st, opts->service, opts->method) ) {
    CF_CRITICAL("send_create_stream_request() fails");
  }
  else {
    channel_state_lock();
    while ( st->state == corpc_stream_opening ) {
      channel_state_wait(-1);
    }
    if ( !( fok = (st->state == corpc_stream_established)) ) {
      CF_CRITICAL("NOT ESTABLISHED: %s", corpc_stream_state_string(st->state));
    }
    channel_state_unlock();
  }

  if ( !fok && st ) {
    destroy_stream(&st);
  }

  return st;
}

void corpc_close_stream(corpc_stream ** stp)
{
  if ( stp && *stp ) {

    corpc_stream * st = *stp;
    corpc_channel * channel = st->channel;

    if ( !send_close_stream_notify(st) ) {
      CF_ERROR("send_close_stream_notify() fails");
    }

    channel_state_lock();
    ccarray_erase_item(&channel->streams, stp);
    channel_state_unlock();
  }
}





ssize_t corpc_stream_read(struct corpc_stream * st, void ** out)
{
  corpc_channel * channel = st->channel;
  struct comsg * comsg = NULL;
  ssize_t size = 0;

  *out = NULL;

  channel_state_lock();

  while ( ccfifo_is_empty(&st->rxq) && corpc_channel_established(channel)
      && (st->state == corpc_stream_established || st->state == corpc_stream_opening) ) {
    channel_state_wait(-1);
  }

  comsg = ccfifo_ppop(&st->rxq);
  channel_state_unlock();


  if ( !comsg ) {
    CF_ERROR("ccfifo_ppop() st=%d fails. st->state=%s", st->sid, corpc_stream_state_string(st->state) );
    goto end;
  }

  if ( comsg->hdr.code != co_msg_data ) {
    CF_CRITICAL("invalid message code %u when expected co_msg_data=%u st=%d", comsg->hdr.code,
        co_msg_data, st->sid);
    goto end;
  }

  if ( !(*out = malloc(comsg->data.hdr.pldsize)) ) {
    CF_CRITICAL("malloc(ccmsg->data) fails: %s", strerror(errno));
    goto end;
  }

  memcpy(*out, comsg->data.details.bits, size = comsg->data.hdr.pldsize);

end:

  if ( comsg ) {
    free(comsg);
  }

  return size;
}

ssize_t corpc_stream_write(struct corpc_stream * st, const void * data, size_t size)
{
  return send_data(st, data, size);
}



