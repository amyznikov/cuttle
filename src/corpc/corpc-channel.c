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

#define CORPC_CHANNEL_THREAD_STACK_SIZE   (256*1024)
#define CORPC_STREAM_DEFAULT_QUEUE_SIZE   8

#define CORPC_STREAM_DEFAULT_STACK_SIZE   (2*1024*1024)



const char * corpc_channel_state_string(enum corpc_channel_state state)
{
  switch (state) {
    case corpc_channel_state_idle:
      return "idle";
    case corpc_channel_state_resolving:
      return "resolving";
    case corpc_channel_state_connecting:
      return "connecting";
    case corpc_channel_state_accepting:
      return "accepting";
    case corpc_channel_state_established:
      return "established";
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



static co_thread_lock_t g_global_lock =
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
  if ( co_thread_broadcast(&g_global_lock) < 0 ) {
    CF_FATAL("co_thread_broadcast() fails: %s", strerror(errno));
  }
}

typedef
struct write_lock {
  bool locked;
} write_lock;

static bool acquire_write_lock(corpc_stream * st, corpc_channel * channel, int tmo, write_lock * wlock)
{
  int64_t ct, et;

  wlock->locked = false;

  if ( tmo >= 0 ) {
    et = cf_get_monotic_ms() + tmo;
  }

  channel_state_lock();

  while ( 42 ) {

    if ( !corpc_channel_established(channel) || (st && st->state != corpc_stream_established) ) {
      errno = ENOTCONN;
      break;
    }

    if ( !channel->write_lock && (!st || st->rwnd > 0) ) {
      channel->write_lock = wlock->locked = true;
      break;
    }


    if ( tmo >= 0 && (ct = cf_get_monotic_ms()) >= et ) {
      errno = ETIME;
      break;
    }

    channel_state_wait(tmo < 0 ? -1 : (int) (et - ct));
  }

  channel_state_unlock();

  return wlock->locked;
}


static void release_write_lock(corpc_channel * channel, write_lock * wlock)
{
  if ( wlock->locked ) {
    channel_state_lock();
    wlock->locked = channel->write_lock = false;
    channel_state_signal();
    channel_state_unlock();
  }
}


// must be locked
void set_channel_state(corpc_channel * channel, enum corpc_channel_state state, int reason, bool lock)
{
  CF_NOTICE("%s -> %s : %s", corpc_channel_state_string(channel->state), corpc_channel_state_string(state),
      strerror(reason));

  if ( lock ) {
    channel_state_lock();
  }

  channel->state = state;
  channel_state_signal();

  if ( lock ) {
    channel_state_unlock();
  }

  if ( channel->onstatechanged ) {
    channel->onstatechanged(channel, state, reason);
  }
}



void corpc_channel_cleanup(struct corpc_channel * channel)
{
  free(channel->connect_opts.connect_address), channel->connect_opts.connect_address = NULL;

  CF_NOTICE("NB_STREAMS=%zu", ccarray_size(&channel->streams));
  ccarray_cleanup(&channel->streams);

  CF_NOTICE("LEAVE");
}


bool corpc_channel_init(struct corpc_channel * channel, const struct corpc_channel_open_args * opts)
{
  bool fok = false;

  channel->state = corpc_channel_state_idle;
  channel->refs = 1;

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

    channel->onconnect = opts->onconnect;
    channel->onstatechanged = opts->onstatechanged;

    channel->services = opts->services;
    channel->ssl_ctx = opts->ssl_ctx;
    channel->keep_alive = opts->keep_alive;
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

void corpc_channel_addref_internal(corpc_channel * channel, bool lock)
{
  if ( channel ) {

    if ( lock ) {
      channel_state_lock();
    }

    ++channel->refs;

    if ( lock ) {
      channel_state_unlock();
    }
  }
}


void corpc_channel_release_internal(corpc_channel ** channel, bool lock)
{
  if ( channel && *channel ) {

    if ( lock ) {
      channel_state_lock();
    }

    CF_DEBUG("(*channel)->refs=%d", (*channel)->refs);

    if ( --(*channel)->refs < 1 && ccarray_size(&(*channel)->streams) < 1 ) {
      channel_destroy(channel);
    }

    if ( lock ) {
      channel_state_unlock();
    }
  }
}



void corpc_channel_close_internal(corpc_channel ** chp, bool lock)
{
  if ( chp && *chp ) {

    corpc_channel * channel = *chp;

    if ( lock ) {
      channel_state_lock();
    }

    co_ssl_socket_close(channel->ssl_sock, false);
    set_channel_state(channel, corpc_channel_state_closed, errno, false);
    corpc_channel_release_internal(&channel, false);

    if ( lock ) {
      channel_state_unlock();
    }

    *chp = NULL;
  }
}


///////////////////////////////////////////////////////////////////////////////////




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


static inline uint16_t srwnd(const corpc_stream * st)
{
  return st ? ccfifo_capacity(&st->rxq) - ccfifo_size(&st->rxq) : 0;
}

static bool send_create_stream_request(corpc_stream * st, const char * service, const char * method)
{
  corpc_channel * channel = st->channel;
  write_lock wlock;
  bool fok = false;

  if ( acquire_write_lock(NULL, channel, -1, &wlock) ) {
    fok = corpc_proto_send_create_stream_request(channel->ssl_sock, st->sid, srwnd(st), service, method);
    release_write_lock(channel, &wlock);
  }

  return fok;
}

static bool send_create_stream_responce(corpc_channel * channel, uint16_t sid, uint16_t did, uint16_t rwnd, uint16_t status)
{
  write_lock wlock;
  bool fok = false;

  if ( acquire_write_lock(NULL, channel, -1, &wlock) ) {
    fok = corpc_proto_send_create_stream_responce(channel->ssl_sock, sid, did, rwnd, status);
    release_write_lock(channel, &wlock);
  }

  return fok;
}

static bool send_close_stream_notify(corpc_stream * st)
{
  corpc_channel * channel = st->channel;
  write_lock wlock;
  bool fok = false;

  if ( acquire_write_lock(NULL, channel, -1, &wlock) ) {
    fok = corpc_proto_send_close_stream(channel->ssl_sock, st->sid, st->did, 0);
    release_write_lock(channel, &wlock);
  }


  return fok;
}

static bool send_data_ack(corpc_stream * st)
{
  corpc_channel * channel = st->channel;
  write_lock wlock;
  bool fok = false;

  if ( acquire_write_lock(NULL, channel, -1, &wlock) ) {
    fok = corpc_proto_send_data_ack(channel->ssl_sock, st->sid, st->did);
    release_write_lock(st->channel, &wlock);
  }


  return fok;
}

static bool send_data(corpc_stream * st, const void * data, size_t size)
{
  corpc_channel * channel = st->channel;
  write_lock wlock;
  bool fok = false;

  if ( acquire_write_lock(st, channel, -1, &wlock) ) {
    if ( (fok = corpc_proto_send_data(channel->ssl_sock, st->sid, st->did, data, size)) ) {
      channel_state_lock();
      --st->rwnd;
      channel_state_unlock();
    }
    release_write_lock(st->channel, &wlock);
  }

  return fok;
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
    st->rwnd = args->rwnd;
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
  return (channel->state == corpc_channel_state_established);
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

static corpc_stream * accept_stream(corpc_channel * channel, uint16_t did, uint16_t rwnd,
    create_stream_responce_code * status)
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
        .did = did,
        .rwnd = rwnd,
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


struct service_method_wrapper_thread_arg {
  corpc_stream * st;
  const struct corpc_service * service;
  const struct corpc_service_method * method;
};

static void service_method_wrapper_thread(void * arg)
{
  struct service_method_wrapper_thread_arg * myarg = arg;
  corpc_stream * st = myarg->st;
  const struct corpc_service * service = myarg->service;
  const struct corpc_service_method * method = myarg->method;
  free(arg);

  CF_DEBUG("C %s/%s()", service->name, method->name);
  method->proc(st);
  CF_DEBUG("R %s/%s()", service->name, method->name);

  corpc_close_stream(&st);

  CF_DEBUG("FINISHED");
}

static bool start_service_method_thread(corpc_stream * st, const struct corpc_service * service,
    const struct corpc_service_method * method)
{
  struct service_method_wrapper_thread_arg * arg;
  bool fok = false;

  if ( !(arg = malloc(sizeof(*arg))) ) {
    CF_CRITICAL("malloc(service_method_wrapper_thread_arg) fails: %s", strerror(errno));
  }
  else {
    arg->st = st;
    arg->service = service;
    arg->method = method;
    if ( !(fok = co_schedule(service_method_wrapper_thread, arg, CORPC_STREAM_DEFAULT_STACK_SIZE)) ) {
      CF_CRITICAL("co_schedule(service_method_wrapper_thread) fails: %s", strerror(errno));
      free(arg);
    }
  }

  return fok;
}



static void on_accepted_thread(void * arg)
{
  corpc_channel * channel = arg;
  CF_DEBUG("STARTED");
  channel->onaccepted(channel);
  CF_DEBUG("FINISHED");
}

static bool start_on_accepted_thread(corpc_channel * channel)
{
  return co_schedule(on_accepted_thread, channel, 1024 * 1024);
}






static bool on_create_stream_request(corpc_channel * channel, comsg ** msgp)
{
  comsg_create_stream_request * rc = &(*msgp)->create_stream_request;

  uint16_t sid = 0;
  uint16_t did = rc->hdr.sid;
  uint16_t rwnd = rc->details.rwnd;
  uint16_t service_name_length = rc->details.service_name_length;
  uint16_t method_name_length = rc->details.method_name_length;

  char service_name[service_name_length + 1];
  char method_name[method_name_length + 1];

  const struct corpc_service * service = NULL;
  const struct corpc_service_method * method = NULL;
  corpc_stream * st = NULL;

  bool fok = true;

  create_stream_responce_code status =
        create_stream_responce_internal_error;


  if( !channel->services ) {
    status = create_stream_responce_no_service;
    goto end;
  }


  memcpy(service_name, rc->details.pack + 0, service_name_length);
  memcpy(method_name, rc->details.pack + service_name_length, method_name_length);

  service_name[service_name_length] = 0;
  method_name[method_name_length] = 0;


  for ( int i = 0; channel->services[i] != NULL; ++i ) {
    if ( strcmp(service_name, channel->services[i]->name) == 0 ) {
      service = channel->services[i];
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

  if ( !(st = accept_stream(channel, did, rwnd, &status)) ) {
    CF_CRITICAL("accept_stream(did=%u) fails", did);
    goto end;
  }

  if ( !start_service_method_thread(st, service, method) ) {
    CF_CRITICAL("start_service_method_thread(%s/%s) fails", service->name, method->name);
    status = create_stream_responce_internal_error;
    goto end;
  }

  sid = st->sid;
  status = create_stream_responce_ok;

end:

  if ( !send_create_stream_responce(channel, sid, did, srwnd(st), status) ) {
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

  return fok;
}


static bool on_create_stream_responce(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;
  const comsg_create_stream_responce * resp = &(*msgp)->create_stream_responce;
  corpc_stream_state state;

  uint16_t sid = resp->hdr.did;
  bool fok = true;

  channel_state_lock();

  if ( !(st = find_stream_by_sid(channel, sid)) ) {
    CF_CRITICAL("find_stream_by_sid(sid=%u) fails", sid);
    fok = false;
    errno = EPROTO;
  }
  else if ( st->state != corpc_stream_opening ) {
    CF_CRITICAL("BUG: Invalid stream state : sid=%u state=%s", sid, corpc_stream_state_string(st->state));
  }
  else {

    switch ( resp->details.status ) {
      case create_stream_responce_ok :
        state = corpc_stream_established;
        st->did = resp->hdr.sid;
        st->rwnd = resp->details.rwnd;
        CF_NOTICE("SET st->rwnd=%u", st->rwnd);
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
        fok = false;
        errno = EPROTO;
      break;
    }

    if ( state != corpc_stream_established ) {
      CF_CRITICAL("Create stream %u fails: %s", sid, corpc_stream_state_string(state));
    }

    corpc_set_stream_state(st, state);
  }

  channel_state_unlock();

  return fok;
}

static bool on_close_stream_notify(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;

  channel_state_lock();

  if ( (st = find_stream_by_sid(channel, (*msgp)->hdr.did)) ) {
    corpc_set_stream_state(st, corpc_stream_closed_by_remote_party);
  }

  channel_state_unlock();
  return true;
}

static bool on_data_message(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;
  bool fok = true;

  channel_state_lock();


  if ( !(st = find_stream_by_sid(channel, (*msgp)->hdr.did)) ) {
    CF_CRITICAL("find_stream_by_sid(did=%u) fails", (*msgp)->hdr.did);
    corpc_set_stream_state(st, corpc_stream_protocol_error);
    fok = false, errno = EPROTO;
  }
  else if ( ccfifo_is_full(&st->rxq) ) {
    // app or party bug
    CF_FATAL("BUG BUG BUG: ccfifo is full for sid=%u", st->sid);
    corpc_set_stream_state(st, corpc_stream_protocol_error);
    fok = false, errno = EPROTO;
  }
  else {
    ccfifo_push(&st->rxq, msgp);
    *msgp = NULL;
    channel_state_signal();
  }

  channel_state_unlock();
  return fok;
}

static bool on_data_ack(corpc_channel * channel, comsg ** msgp)
{
  corpc_stream * st;
  const comsg_data_ack * msg = &(*msgp)->data_ack;

  channel_state_lock();

  if ( (st = find_stream_by_sid(channel, msg->hdr.did)) ) {
    ++st->rwnd;
    channel_state_signal();
  }

  channel_state_unlock();
  return true;
}




static void corpc_channel_thread(void * arg)
{
  struct corpc_channel * channel = arg;
  comsg * msg = NULL;

  bool fok = true;

  channel_state_lock();
  corpc_channel_addref_internal(channel, false);

  if ( channel->state == corpc_channel_state_connecting ) {
    // actual ssl handshake already made in corpc_channel_open()
    set_channel_state(channel, corpc_channel_state_established, 0, false);
  }

  else if ( channel->state == corpc_channel_state_accepting ) {

    channel_state_unlock();
    if ( !(fok = co_ssl_accept(channel->ssl_sock)) ) {
      CF_CRITICAL("co_ssl_socket_accept() fails");
    }
    else if ( channel->onaccept && !(fok = channel->onaccept(channel)) ) {
      CF_CRITICAL("channel->onaccept() fails");
    }
    channel_state_lock();

    if ( fok ) {
      set_channel_state(channel, corpc_channel_state_established, 0, false);
    }
  }
  else {
    CF_FATAL("Unexpected channel state %s", corpc_channel_state_string(channel->state));
  }

  if ( !fok ) {
    goto end;
  }

  co_ssl_socket_set_recv_timeout(channel->ssl_sock, -1);
  channel_state_unlock();


  if ( !(msg = malloc(sizeof(*msg))) ) {
    CF_FATAL("malloc(msg) fails: %s", strerror(errno));
  }

  if ( channel->onaccepted && !start_on_accepted_thread(channel) ) {
    CF_FATAL("FIXME: NOT HANDLED: start_on_accepted_thread() fails: %s", strerror(errno));
  }

  while ( corpc_proto_recv_msg(channel->ssl_sock, msg) ) {

    switch ( msg->hdr.code ) {

      case co_msg_create_stream_req :
        fok = on_create_stream_request(channel, &msg);
      break;

      case co_msg_create_stream_resp :
        fok = on_create_stream_responce(channel, &msg);
      break;

      case co_msg_close_stream_req :
        fok = on_close_stream_notify(channel, &msg);
      break;

      case co_msg_data :
        fok = on_data_message(channel, &msg);
      break;

      case co_msg_data_ack :
        fok = on_data_ack(channel, &msg);
      break;

      default :
        CF_CRITICAL("Unknown message code received: %u", msg->hdr.code);
        fok = false;
        errno = EPROTO;
      break;
    }

    if ( !fok ) {
      break;
    }

    if ( !msg && !(msg = malloc(sizeof(*msg))) ) {
      CF_FATAL("malloc(msg) fails");
    }
  }

  if ( fok ) {
    CF_CRITICAL("corpc_proto_recv_msg() fails: %s", strerror(errno));
  }

  channel_state_lock();

end :

  corpc_channel_close_internal(&channel, false);
  channel_state_unlock();

  free(msg);

  CF_INFO("FINIDHED channel=%p", channel);
}





corpc_channel * corpc_channel_new(const corpc_channel_open_args  * opts)
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
      corpc_channel_close(channel);
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

const SSL * corpc_channel_get_ssl(const corpc_channel * channel)
{
  return co_ssl_socket_get_ssl(channel->ssl_sock);
}

static bool ssl_server_connect(corpc_channel * channel)
{
  struct addrinfo * ai = NULL;
  co_ssl_socket * ssl_sock = NULL;
  bool fok = false;

  set_channel_state(channel, corpc_channel_state_resolving, 0, true);

  if ( !co_server_resolve(&ai, channel->connect_opts.connect_address, channel->connect_opts.connect_port,
      channel->connect_opts.connect_tmout_ms) ) {
    CF_CRITICAL("co_server_resolve() fails: %s", strerror(errno));
    goto end;
  }

  if ( !(ssl_sock = co_ssl_socket_create_new(ai->ai_family, SOCK_STREAM, IPPROTO_TCP, channel->ssl_ctx)) ) {
    CF_CRITICAL("co_ssl_socket_create_new() fails : %s ", strerror(errno));
    goto end;
  }

  channel_state_lock();
  channel->ssl_sock = ssl_sock;
  set_channel_state(channel, corpc_channel_state_connecting, 0, false);
  channel_state_unlock();

  if ( !co_ssl_socket_connect(channel->ssl_sock, ai->ai_addr, channel->connect_opts.connect_tmout_ms) ) {
    CF_CRITICAL("co_ssl_connect() fails");
    goto end;
  }

  if ( channel->onconnect && !channel->onconnect(channel) )  {
    CF_CRITICAL("channel->onconnect() returns false");
    goto end;
  }


  channel_state_lock();
  if ( !co_schedule(corpc_channel_thread, channel, CORPC_CHANNEL_THREAD_STACK_SIZE) ) {
    CF_CRITICAL("co_schedule(corpc_channel_thread) fails: %s", strerror(errno));
  }
  else {
    while ( channel->state == corpc_channel_state_connecting ) {
      channel_state_wait(-1);
    }
    if ( !(fok = corpc_channel_established(channel)) ) {
      CF_CRITICAL("NOT ESTABLISHED: %s", corpc_channel_state_string(channel->state));
    }
  }
  channel_state_unlock();

end:

  if ( ai ) {
    freeaddrinfo(ai);
  }

  if ( !fok ) {
    channel_state_lock();
    co_ssl_socket_destroy(&channel->ssl_sock, true);
    set_channel_state(channel, corpc_channel_state_idle, errno, false);
    channel_state_unlock();
  }

  return (channel->ssl_sock != NULL);
}

corpc_channel * corpc_channel_open(const struct corpc_channel_open_args * args)
{
  corpc_channel * channel = NULL;

  if ( !(channel = corpc_channel_new(args)) ) {
    CF_CRITICAL("corpc_channel_new() fails: %s", strerror(errno));
  }
  else if ( !ssl_server_connect(channel) ) {
    CF_CRITICAL("ssl_server_connect() fails: %s", strerror(errno));
    channel_destroy(&channel);
  }

  return channel;
}


bool corpc_channel_accept(corpc_listening_port * clp, co_ssl_socket * accepted_sock)
{
  corpc_channel * channel = NULL;

  if ( !(channel = corpc_channel_new(NULL)) ) {
    CF_CRITICAL("corpc_channel_new() fails: %s", strerror(errno));
  }
  else {

    channel->refs = 0;
    channel->state = corpc_channel_state_accepting;
    channel->ssl_sock = accepted_sock;
    channel->services = clp->services;
    channel->keep_alive = clp->keep_alive;
    channel->ssl_ctx = clp->base.ssl_ctx;
    channel->onaccept = clp->onaccept;
    channel->onaccepted = clp->onaccepted;

    if ( !co_schedule(corpc_channel_thread, channel, CORPC_CHANNEL_THREAD_STACK_SIZE) ) {
      CF_CRITICAL("co_schedule(corpc_channel_thread) fails: %s", strerror(errno));
      channel_destroy(&channel);
    }
  }

  return channel != NULL;
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


corpc_stream * corpc_open_stream(corpc_channel * channel, const corpc_open_stream_opts * opts)
{
  corpc_stream * st = NULL;

  bool fok = false;

  if ( channel->state != corpc_channel_state_established ) {
    CF_CRITICAL("Invalid channel state: %s", corpc_channel_state_string(channel->state));
    errno = ENOTCONN;
  }
  else if ( !(st = create_new_stream(channel)) ) {
    CF_CRITICAL("create_new_stream() fails");
  }
  else if ( !send_create_stream_request(st, opts->service, opts->method) ) {
    CF_CRITICAL("send_create_stream_request() fails");
  }
  else {
    channel_state_lock();
    while ( corpc_channel_established(channel) && st->state == corpc_stream_opening ) {
      channel_state_wait(-1);
    }
    if ( !(fok = (st->state == corpc_stream_established)) ) {
      CF_CRITICAL("NOT ESTABLISHED: %s", corpc_stream_state_string(st->state));
    }
    channel_state_unlock();
  }

  if ( !fok && st ) {
    channel_state_lock();
    ccarray_erase_item(&channel->streams, &st);
    channel_state_unlock();
    corpc_stream_destroy(&st);
  }

  return st;
}



static bool corpc_stream_read_internal(struct corpc_stream * st, struct comsg ** out)
{
  corpc_channel * channel = st->channel;
  bool is_connected;

  *out = NULL;

  channel_state_lock();

  while ( ccfifo_is_empty(&st->rxq) && corpc_channel_established(channel)
      && (st->state == corpc_stream_established || st->state == corpc_stream_opening) ) {
    channel_state_wait(-1);
  }

  *out = ccfifo_ppop(&st->rxq);
  is_connected = corpc_channel_established(channel) && (st->state == corpc_stream_established);

  channel_state_unlock();


  if ( *out ) {
    if ( (*out)->hdr.code != co_msg_data ) {
      CF_CRITICAL("invalid message code %u when expected co_msg_data=%u st=%d", (*out)->hdr.code, co_msg_data, st->sid);
      free(*out), *out = NULL;
    }
    else if ( is_connected && !send_data_ack(st) ) {
      CF_CRITICAL("send_data_ack() fails");
    }
  }

  return *out != NULL;
}

ssize_t corpc_stream_read(struct corpc_stream * st, void ** out)
{
  struct comsg * comsg = NULL;
  ssize_t size = -1;

  *out = NULL;

  if ( corpc_stream_read_internal(st, &comsg) ) {

    if ( !(*out = malloc(comsg->data.hdr.pldsize)) ) {
      CF_CRITICAL("malloc(ccmsg->data) fails: %s", strerror(errno));
    }
    else {
      memcpy(*out, comsg->data.details.bits, size = comsg->data.hdr.pldsize);
    }

    free(comsg);
  }

  return size;
}

bool corpc_stream_read_msg(struct corpc_stream * st, bool (*unpack)(void *, const void *, size_t), void * appmsg)
{
  struct comsg * comsg = NULL;
  bool fok = false;

  if ( corpc_stream_read_internal(st, &comsg) ) {
    fok = unpack(appmsg, comsg->data.details.bits, comsg->hdr.pldsize);
  }

  free(comsg);
  return fok;
}


bool corpc_stream_write(struct corpc_stream * st, const void * data, size_t size)
{
  return send_data(st, data, size);
}

bool corpc_stream_write_msg(struct corpc_stream * st, size_t (*pack)(const void *, void **), const void * appmsg)
{
  void * data = NULL;
  size_t size;
  bool fok = false;
  if ( (size = pack(appmsg, &data)) > 0 ) {
    fok = send_data(st, data, size);
  }
  free(data);
  return fok;
}



void corpc_channel_addref(corpc_channel * channel)
{
  corpc_channel_addref_internal(channel, true);
}

void corpc_channel_release(corpc_channel ** channel)
{
  corpc_channel_release_internal(channel, true);
}

void corpc_channel_close(corpc_channel ** chp)
{
  corpc_channel_close_internal(chp, true);
}

void corpc_close_stream(corpc_stream ** stp)
{
  corpc_stream * st;
  corpc_channel * channel;

  if ( stp && (st = *stp) ) {

    if ( (channel = st->channel) ) {

      send_close_stream_notify(st);

      channel_state_lock();
      ccarray_erase_item(&channel->streams, stp);
      if ( ccarray_size(&channel->streams) < 1 && channel->refs < 1 ) {
        channel_destroy(&channel);
      }
      channel_state_unlock();
    }

    corpc_stream_destroy(stp);
  }
}
