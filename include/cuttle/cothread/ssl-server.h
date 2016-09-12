/*
 * ssl-server.h
 *
 *  Created on: Sep 9, 2016
 *      Author: amyznikov
 */

// #pragma once

#ifndef __cuttle_ssl_server_h__
#define __cuttle_ssl_server_h__

#include <cuttle/sockopt.h>
#include <cuttle/ccarray.h>
#include <cuttle/cothread/ssl.h>
#include <cuttle/cothread/ssl-listening-port.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef
struct co_ssl_server {
  ccarray_t ssl_ports;
  SSL_CTX * ssl_ctx;
} co_ssl_server;


typedef
struct co_ssl_server_opts {
  SSL_CTX * ssl_ctx;
  int max_nb_ports;
  int extra_obj_size;
} co_ssl_server_opts;


bool co_ssl_server_init(struct co_ssl_server * sslsrv, const co_ssl_server_opts * opts);
void co_ssl_server_cleanup(struct co_ssl_server * sslsrv);

co_ssl_server * co_ssl_server_new(const co_ssl_server_opts * opts);
void co_ssl_server_destroy(co_ssl_server ** ssrv);

bool co_ssl_server_add_port(co_ssl_server * ssrv, co_ssl_listening_port * sslp);
co_ssl_listening_port * co_ssl_server_add_new_port(co_ssl_server * ssrv, const co_ssl_listening_port_opts * opts);

bool co_ssl_server_start(co_ssl_server * ssrv);
bool co_ssl_server_stop(co_ssl_server * ssrv);




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_ssl_server_h__ */
