/*
 * ssl-listening-port.h
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

// #pragma once

#ifndef __cuttle_cothread_ssl_listening_port_h__
#define __cuttle_cothread_ssl_listening_port_h__

#include <cuttle/cothread/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef
struct co_ssl_listening_port {
  sockaddr_type listen_address;
  SSL_CTX * ssl_ctx;
  int sock_type;
  int proto;
  bool(*onaccept)(struct co_ssl_listening_port * sslp,
      co_socket * accepted_sock);
  void * cookie;
  size_t accepted_stack_size;
  co_socket * listening_sock;
} co_ssl_listening_port;

typedef
struct co_ssl_listening_port_opts {
  sockaddr_type bind_address;
  SSL_CTX * ssl_ctx;
  int sock_type;
  int proto;
  bool(*onaccept)(struct co_ssl_listening_port * sslp,
      co_socket * accepted_sock);
  void * cookie;
  size_t extra_object_size;
  size_t accepted_stack_size;
} co_ssl_listening_port_opts;

bool co_ssl_listening_port_init(struct co_ssl_listening_port * sslp, const struct co_ssl_listening_port_opts * opts);
void co_ssl_listening_port_cleanup(struct co_ssl_listening_port * sslp);
struct co_ssl_listening_port * co_ssl_listening_port_new(const struct co_ssl_listening_port_opts * opts);
void co_ssl_listening_port_release(struct co_ssl_listening_port ** sslpp);
void * co_ssl_get_listening_port_cookie(const struct co_ssl_listening_port * sslp);
bool co_ssl_listening_port_start_listen(struct co_ssl_listening_port * sslp);

#ifdef __cplusplus
}
#endif

#endif /* __cuttle_cothread_ssl_listening_port_h__ */
