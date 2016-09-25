/*
 * src/corpc/channel.h
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_src_corpc_channel_h__
#define __cuttle_src_corpc_channel_h__

#include <cuttle/corpc/channel.h>
#include <cuttle/ccarray.h>
#include <cuttle/ccfifo.h>
#include "corpc-listening-port.h"

#ifdef __cplusplus
extern "C" {
#endif


struct corpc_stream {
  corpc_channel * channel;
  ccfifo rxq;
  corpc_stream_state state;
  uint16_t sid;
  uint16_t did;
  uint16_t rwnd;
};

typedef
struct corpc_stream_opts {
  corpc_channel * channel;
  corpc_stream_state state;
  uint16_t sid;
  uint16_t did;
  uint16_t rwnd;
} corpc_stream_opts;





struct corpc_channel {

  co_ssl_socket * ssl_sock;
  void * client_context;
  ccarray_t streams; // <corpc_stream*>

  const struct corpc_service ** services;
  SSL_CTX * ssl_ctx;

  int refs;
  bool streams_lock;
  bool write_lock;
  bool state_lock;
  corpc_channel_state state;

  void (*onstatechanged)(struct corpc_channel * channel,
      enum corpc_channel_state,
      int reason);

  bool (*onconnect)(const corpc_channel * channel);
  bool (*onaccept)(const corpc_channel * channel);
  void (*onaccepted)(corpc_channel * channel);

  struct {
    char * connect_address;
    uint16_t connect_port;
    int connect_tmout_ms;
  } connect_opts;

  struct so_keepalive_opts
    keep_alive;
};

corpc_channel * corpc_channel_new(const struct corpc_channel_open_args * opts);

bool corpc_channel_accept(corpc_listening_port * sslp, co_ssl_socket * ssl_sock);
void corpc_channel_addref(corpc_channel * chp);
void corpc_channel_relase(corpc_channel ** chp);

enum corpc_channel_state corpc_get_channel_state(const corpc_channel * channel);
bool corpc_channel_established(const corpc_channel * channel);


bool corpc_stream_init(struct corpc_stream * st, const corpc_stream_opts * args);
corpc_stream * corpc_stream_new(const corpc_stream_opts * args);
void corpc_stream_cleanup(struct corpc_stream * st);

//void corpc_stream_


void corpc_set_stream_state(struct corpc_stream * st, corpc_stream_state state);




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_channel_h__ */
