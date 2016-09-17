/*
 * cuttle/cothread/co-ssl.h
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 *
 *  SSL BIO for cothread
 */

// #pragma once

#ifndef __cuttle_co_ssl_h__
#define __cuttle_co_ssl_h__

#include <cuttle/sockopt.h>
#include <cuttle/ssl/ssl-context.h>
#include <cuttle/cothread/scheduler.h>


#ifdef __cplusplus
extern "C" {
#endif

BIO_METHOD * BIO_co_socket(void);
BIO * BIO_co_socket_new(co_socket * cc);

SSL * co_ssl_new(SSL_CTX * ssl_ctx, co_socket * cc);
void co_ssl_free(SSL ** ssl);



typedef
struct co_ssl_socket
  co_ssl_socket;

co_ssl_socket * co_ssl_socket_new(int af, int sock_type, int proto, SSL_CTX * ssl_ctx);
co_ssl_socket * co_ssl_socket_attach(co_socket * cc, SSL_CTX * ssl_ctx); /* takes ownership of cc */
void co_ssl_socket_close(co_ssl_socket ** ssl_sock, bool abort_conn);

bool co_ssl_socket_set_send_timeout(co_ssl_socket * ssl_sock, int msec);
bool co_ssl_socket_set_recv_timeout(co_ssl_socket * ssl_sock, int msec);

ssize_t co_ssl_socket_send(co_ssl_socket * ssl_sock, const void * buf, size_t size);
ssize_t co_ssl_socket_recv(co_ssl_socket * ssl_sock, void * buf, size_t size);


typedef
struct co_ssl_connect_opts {
  SSL_CTX * ssl_ctx;
  int sock_type;
  int proto;
  int tmout;
} co_ssl_connect_opts;

bool co_ssl_connect(co_ssl_socket * ssl_sock, const struct sockaddr * addrs, int tmo_ms);
bool co_ssl_server_connect(co_ssl_socket * ssl_sock, const char * address, uint16_t port, int tmo_ms);

co_ssl_socket * co_ssl_connect_new(const struct sockaddr * addrs, const struct co_ssl_connect_opts * opts);
co_ssl_socket * co_ssl_server_connect_new(const char * address, uint16_t port, const struct co_ssl_connect_opts * opts);

bool co_ssl_socket_accept(co_ssl_socket * cc);
co_ssl_socket * co_ssl_socket_accept_new(co_socket ** cc, SSL_CTX * ssl_ctx);

co_socket * co_ssl_listen(const struct sockaddr * addrs, int sock_type, int proto);

const SSL * co_ssl_socket_get_ssl(const co_ssl_socket * ssl_sock);

#ifdef __cplusplus
}
#endif

#endif /* __cuttle_co_ssl_h__ */
