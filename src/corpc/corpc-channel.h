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
};

typedef
struct corpc_stream_opts {
  corpc_channel * channel;
  corpc_stream_state state;
  uint16_t sid;
  uint16_t did;
} corpc_stream_opts;





struct corpc_channel {

  co_ssl_socket * ssl_sock;
  void * client_context;
  ccarray_t streams; // <corpc_stream*>

  int refs;

  corpc_channel_state state;
  void (*onstatechanged)(struct corpc_channel * channel,
      enum corpc_channel_state,
      int reason);

  struct {
    char * connect_address;
    uint16_t connect_port;
    int connect_tmout_ms;
    SSL_CTX * ssl_ctx;
  } connect_opts;

  struct {
    bool (*onaccepted)(struct corpc_channel * channel);
    void (*ondisconnected)(struct corpc_channel * channel);
    SSL_CTX * ssl_ctx;
    const struct corpc_service ** services;
    int nb_services;
  } listen_opts;

};


corpc_channel * corpc_channel_accept(corpc_listening_port * sslp, const co_socket * ssl_sock);

enum corpc_channel_state corpc_get_channel_state(const corpc_channel * channel);
bool corpc_channel_established(const corpc_channel * channel);


bool corpc_channel_read(struct corpc_stream * st, corpc_msg * msg);
bool corpc_channel_write(struct corpc_stream * st, const corpc_msg * msg);



bool corpc_stream_init(struct corpc_stream * st, const corpc_stream_opts * args);
corpc_stream * corpc_stream_new(const corpc_stream_opts * args);
void corpc_stream_cleanup(struct corpc_stream * st);


void corpc_set_stream_state(struct corpc_stream * st, corpc_stream_state state);




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_channel_h__ */
