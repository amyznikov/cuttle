/*
 * co-ssl.c
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

#include <malloc.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <cuttle/sockopt.h>
#include <cuttle/debug.h>
#include <cuttle/ssl/error.h>
#include <cuttle/cothread/resolve.h>
#include <cuttle/cothread/ssl.h>
#include "co-scheduler.h"

//////////////////////////////////////////////////////////////////////////

static int bio_co_socket_read(BIO * bio, char * buf, int size)
{
  return (buf ? co_socket_recv(bio->ptr, buf, size, 0) : 0);
}

static int bio_co_socket_write(BIO * bio, const char * buf, int size)
{
  return co_socket_send(bio->ptr, buf, size, 0);
}

static int bio_co_socket_puts(BIO * bio, const char * str)
{
  return bio_co_socket_write(bio, str, strlen(str));
}

static long bio_co_socket_ctrl(BIO * bio, int cmd, long arg1, void *arg2)
{
  (void) (bio);
  (void) (arg1);
  (void) (arg2);

  long status = 1;

  switch ( cmd ) {
    case BIO_CTRL_PUSH :
      case BIO_CTRL_POP :
      case BIO_CTRL_FLUSH :
      break;
    default :
      status = 0;
    break;
  }

  return status;
}


BIO_METHOD * BIO_co_socket(void)
{
  static BIO_METHOD methods_bio_co_socket = {
    .type = BIO_TYPE_NULL,
    .name = "bio_co_socket",
    .bwrite = bio_co_socket_write,
    .bread = bio_co_socket_read,
    .bputs = bio_co_socket_puts,
    .bgets = NULL,
    .ctrl = bio_co_socket_ctrl,
    .create = NULL,
    .destroy = NULL,
    .callback_ctrl = NULL,
  };

  return &methods_bio_co_socket;
}

BIO * BIO_co_socket_new(co_socket * cc)
{
  BIO * bio;
  if ( (bio = BIO_new(BIO_co_socket())) ) {
    bio->ptr = cc;
    bio->init = 1;
    bio->num = 0;
    bio->flags = 0;
  }
  return bio;
}


//////////////////////////////////////////////////////////////////////////


SSL * co_ssl_new(SSL_CTX * ssl_ctx, co_socket * cc)
{
  SSL * ssl = NULL;
  BIO * bio = NULL;
  bool fok = false;

  if ( !(ssl = SSL_new(ssl_ctx)) ) {
    goto end;
  }

  if ( !(bio = BIO_co_socket_new(cc)) ) {
    goto end;
  }

  SSL_set_bio(ssl, bio, bio);
  fok = true;

end :
  if ( !fok ) {
    SSL_free(ssl);
    ssl = NULL;
  }

  return ssl;
}

void co_ssl_free(SSL ** ssl)
{
  if ( ssl && *ssl ) {
    SSL_free(*ssl);
    * ssl = NULL;
  }
}

//////////////////////////////////////////////////////////////////////////


struct co_ssl_socket {
  co_socket cc;
  SSL * ssl;
};

bool co_ssl_socket_init(co_ssl_socket * cc, int so)
{
  cc->ssl = NULL;
  return co_socket_init(&cc->cc, so);
}

co_ssl_socket * co_ssl_socket_init_new(int so)
{
  co_ssl_socket * ssl_sock;
  if ( (ssl_sock = malloc(sizeof(*ssl_sock))) && !co_ssl_socket_init(ssl_sock, so) ) {
    free(ssl_sock), ssl_sock = NULL;
  }
  return ssl_sock;
}


bool co_ssl_socket_create(co_ssl_socket * cc, int af, int sock_type, int proto, SSL_CTX * ssl_ctx)
{
  bool fok = false;

  cc->ssl = NULL;

  if ( !co_socket_create(&cc->cc, af, sock_type, proto) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_create() fails: %s", strerror(errno));
  }
  else if ( ssl_ctx && !(cc->ssl = co_ssl_new(ssl_ctx, &cc->cc)) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC, "co_ssl_new() fails: %s", strerror(errno));
    co_socket_close(&cc->cc, false);
  }
  else {
    fok = true;
  }

  return fok;
}

co_ssl_socket * co_ssl_socket_create_new(int af, int sock_type, int proto, SSL_CTX * ssl_ctx)
{
  co_ssl_socket * ssl_sock = NULL;
  if ( (ssl_sock = malloc(sizeof(*ssl_sock))) && !co_ssl_socket_create(ssl_sock, af, sock_type, proto, ssl_ctx) ) {
    free(ssl_sock), ssl_sock = NULL;
  }
  return ssl_sock;
}


bool co_ssl_socket_get_peername(const co_ssl_socket * cc, struct sockaddr * addrs, socklen_t * addrslen)
{
  if ( !cc ) {
    errno = ENOTSOCK;
    return false;
  }
  return co_socket_get_peername(&cc->cc, addrs, addrslen);
}

bool co_ssl_socket_get_sockname(const co_ssl_socket * cc, struct sockaddr * addrs, socklen_t * addrslen)
{
  if ( !cc ) {
    errno = ENOTSOCK;
    return false;
  }
  return co_socket_get_sockname(&cc->cc, addrs, addrslen);
}


co_socket * co_ssl_socket_listen_new(const struct sockaddr * addrs, int sock_type, int proto)
{
  co_socket * cc;
  if ( !(cc = co_socket_create_listening_new(addrs, sock_type, proto)) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_create_listening_new() fails: %s", strerror(errno));
  }
  return cc;
}



bool co_ssl_socket_accept(co_socket * listenning, co_ssl_socket * accepted, SSL_CTX * ssl_ctx, struct sockaddr * addrs, socklen_t * addrslen)
{
  bool fok = false;

  if ( !co_ssl_socket_init(accepted, -1) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC, "co_ssl_socket_init() fails: %s", strerror(errno));
    goto end;
  }

  if ( !co_socket_accept(listenning, &accepted->cc, addrs, addrslen) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_accept() fails: %s", strerror(errno));
    goto end;
  }

  if ( ssl_ctx && !(accepted->ssl = co_ssl_new(ssl_ctx, &accepted->cc)) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC, "co_ssl_new() fails: %s", strerror(errno));
    goto end;
  }

  fok = true;

end:

  if ( !fok ) {
    co_ssl_socket_close(accepted, true);
  }

  return accepted;
}

co_ssl_socket * co_ssl_socket_accept_new(co_socket *listenning, SSL_CTX * ssl_ctx, struct sockaddr * addrs,
    socklen_t * addrslen)
{
  co_ssl_socket * cc = NULL;
  if ( (cc = malloc(sizeof(*cc))) && !co_ssl_socket_accept(listenning, cc, ssl_ctx, addrs, addrslen) ) {
    free(cc), cc = NULL;
  }
  return cc;
}


bool co_ssl_socket_connect(co_ssl_socket * cc, const struct sockaddr * addrs, int tmo_ms)
{
  bool fok = false;
  int ssl_status;

  if ( !co_socket_connect(&cc->cc, addrs, tmo_ms) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_connect() fails: %s", strerror(errno));
  }
  else if ( cc->ssl && (ssl_status = SSL_connect(cc->ssl)) != 1 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_connect() fails: ssl_status=%d %s errno=%s", ssl_status,
        cf_get_ssl_error_string(cc->ssl, ssl_status), strerror(errno));
  }
  else {
    fok = true;
  }

  return fok;
}

co_ssl_socket * co_ssl_socket_connect_new(const struct sockaddr * addrs, SSL_CTX * ssl_ctx, int sock_type, int proto, int tmo)
{
  co_ssl_socket * cc = NULL;
  if ( (cc = co_ssl_socket_create_new(addrs->sa_family, sock_type, proto, ssl_ctx))
      && !co_ssl_socket_connect(cc, addrs, tmo) ) {
    co_ssl_socket_destroy(&cc, true);
  }
  return cc;
}


void co_ssl_socket_close(co_ssl_socket * ssl_sock, bool abort_conn)
{
  if ( ssl_sock ) {
    co_socket_close(&ssl_sock->cc, abort_conn);
  }
}

void co_ssl_socket_destroy(co_ssl_socket ** ssl_sock, bool abort_conn)
{
  if ( ssl_sock && *ssl_sock ) {
    co_socket_close(&(*ssl_sock)->cc, abort_conn);
    co_ssl_free(&(*ssl_sock)->ssl);
    free(*ssl_sock), *ssl_sock = NULL;
  }
}









const SSL * co_ssl_socket_get_ssl(const co_ssl_socket * ssl_sock)
{
  return ssl_sock ? ssl_sock->ssl : NULL;
}



bool co_ssl_socket_set_send_timeout(co_ssl_socket * ssl_sock, int msec)
{
  bool fok = false;
  if ( !ssl_sock ) {
    errno = EBADF;
  }
  else {
    fok = co_socket_set_send_tmout(&ssl_sock->cc, msec);
  }
  return fok;
}

bool co_ssl_socket_set_recv_timeout(co_ssl_socket * ssl_sock, int msec)
{
  bool fok = false;
  if ( !ssl_sock ) {
    errno = EBADF;
  }
  else {
    fok = co_socket_set_recv_tmout(&ssl_sock->cc, msec);
  }
  return fok;
}

ssize_t co_ssl_socket_send(co_ssl_socket * ssl_sock, const void * buf, size_t size)
{
  ssize_t bytes_sent = -1;

  if ( !ssl_sock ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "ssl_sock is NULL");
    errno = EBADF;
  }
  else if ( !buf ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "buf is NULL");
    errno = EINVAL;
  }
  else if ( ssl_sock->ssl ) {
    bytes_sent = SSL_write(ssl_sock->ssl, buf, size);
  }
  else if ( (bytes_sent = co_socket_send(&ssl_sock->cc, buf, size, 0)) < 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_send() fails: %s", strerror(errno));
  }

  return bytes_sent;
}

ssize_t co_ssl_socket_recv(co_ssl_socket * ssl_sock, void * buf, size_t size)
{
  ssize_t bytes_received = -1;

  if ( !ssl_sock ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "ssl_sock is NULL");
    errno = EBADF;
  }
  else if ( !buf ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "buf is NULL");
    errno = EINVAL;
  }
  else if ( ssl_sock->ssl ) {
    if ( (bytes_received = SSL_read(ssl_sock->ssl, buf, size)) <= 0 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_read() fails");
    }
  }
  else if ( (bytes_received = co_socket_recv(&ssl_sock->cc, buf, size, 0)) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_recv() fails: %s", strerror(errno));
  }

  return bytes_received;
}




bool co_ssl_accept(co_ssl_socket * ssl_sock)
{
  bool fok = false;

  if ( !ssl_sock ) {
    errno = EBADF;
  }
  else if ( ssl_sock->ssl && SSL_accept(ssl_sock->ssl) != 1 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_accept() fails");
    errno = EACCES;
  }
  else {
    fok = true;
  }

  return fok;
}





co_ssl_socket * co_ssl_connect_new(const struct sockaddr * addrs, const struct co_ssl_connect_opts * opts)
{
  co_ssl_socket * ssl_sock = NULL;

  if ( (ssl_sock = co_ssl_socket_create_new(addrs->sa_family, opts->sock_type, opts->proto, opts->ssl_ctx)) ) {
    if ( !co_ssl_socket_connect(ssl_sock, addrs, opts->tmout) ) {
      co_ssl_socket_destroy(&ssl_sock, true);
    }
  }

  return ssl_sock;
}



co_ssl_socket * co_ssl_server_connect_new(const char * address, uint16_t port, const struct co_ssl_connect_opts * opts)
{
  struct addrinfo * ai = NULL;
  co_ssl_socket * ssl_sock = NULL;

  if ( co_server_resolve(&ai, address, port, opts->tmout ? opts->tmout : 15 * 1000) ) {
    ssl_sock = co_ssl_connect_new(ai->ai_addr, opts);
  }

  if ( ai ) {
    freeaddrinfo(ai);
  }

  return ssl_sock;
}




/*
 * void co_ssl_socket_free(co_ssl_socket ** ssl_sock)
{
  if ( ssl_sock && *ssl_sock ) {
    free(*ssl_sock);
    *ssl_sock = NULL;
  }
}
 *
 */
