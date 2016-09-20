/*
 * corpc/channel.h
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_corpc_channel_h__
#define __cuttle_corpc_channel_h__

#include <cuttle/cothread/ssl.h>
#include <cuttle/corpc/service.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef
struct corpc_channel
  corpc_channel;


typedef
struct corpc_stream
  corpc_stream;



typedef
enum corpc_channel_state {
  corpc_channel_state_idle = 0,
  corpc_channel_state_resolving = 1,
  corpc_channel_state_connecting = 2,
  corpc_channel_state_connected = 3,
  corpc_channel_state_accepting = 4,
  corpc_channel_state_accepted = 5,
  corpc_channel_state_disconnecting = 6,
  corpc_channel_state_closed = 7,
} corpc_channel_state;

const char * corpc_channel_state_string(
    enum corpc_channel_state state);




typedef
enum corpc_stream_state {
  corpc_stream_created = 0,
  corpc_stream_opening,
  corpc_stream_too_many_streams,
  corpc_stream_no_such_service,
  corpc_stream_no_such_method,
  corpc_stream_local_internal_error,
  corpc_stream_remote_internal_error,
  corpc_stream_protocol_error,
  corpc_stream_established,
  corpc_stream_closed,
  corpc_stream_closed_by_remote_party,
} corpc_stream_state;

const char * corpc_stream_state_string(
    enum corpc_stream_state state);










typedef
struct corpc_channel_opts {
  const char * connect_address;
  uint16_t connect_port;
  int connect_tmout_ms;
  SSL_CTX * ssl_ctx;
  const struct corpc_service ** services;
  bool (*onconnect)(const corpc_channel * channel);
  void (*onstatechanged)(corpc_channel * channel, enum corpc_channel_state, int reason);
} corpc_channel_opts;



typedef
struct corpc_open_stream_opts {
  const char * service;
  const char * method;
  void (*onstatechanged)(corpc_stream * st,
      enum corpc_stream_state,
      int reason);
} corpc_open_stream_opts;


corpc_channel * corpc_channel_new(const struct corpc_channel_opts * opts);

bool corpc_channel_open(corpc_channel * channel);

void corpc_channel_close(corpc_channel ** channel);

enum corpc_channel_state corpc_get_channel_state(const corpc_channel * channel);

bool corpc_channel_established(const corpc_channel * channel);

void corpc_channel_set_client_context(corpc_channel * channel, void * client_context);
void * corpc_channel_get_client_context(const corpc_channel * channel);
const SSL * corpc_channel_get_ssl(const corpc_channel * channel);


corpc_stream * corpc_open_stream(corpc_channel * channel, const corpc_open_stream_opts * opts);
void corpc_close_stream(corpc_stream ** stp);

ssize_t corpc_stream_read(struct corpc_stream * st, void ** out);
bool corpc_stream_write(struct corpc_stream * st, const void * data, size_t size);


bool corpc_stream_read_msg(struct corpc_stream * st, bool (*unpack)(void * msg, const void * data, size_t size), void * appmsg);
bool corpc_stream_write_msg(struct corpc_stream * st, ssize_t (*pack)(const void * appmsg, void ** data), const void * appmsg);

enum corpc_stream_state corpc_get_stream_state(const corpc_stream * stream);

void * corpc_stream_get_channel_client_context(const corpc_stream * stream);


//void corpc_stream_addref(corpc_stream * st);
//void corpc_stream_release(corpc_stream ** st);




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_channel_h__ */
