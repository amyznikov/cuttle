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
  co_socket * cc;
  SSL * ssl;
};


//static bool is_valid_descriptor(int so) {
//  return (so != -1) && (fcntl(so, F_GETFD) != -1 ||
//      errno != EBADF);
//}


co_ssl_socket * co_ssl_socket_new(co_socket * cc, SSL_CTX * ssl_ctx)
{
  co_ssl_socket * ssl_sock = NULL;
  bool fok = false;

  if ( !cc ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "cc is null");
    goto end;
  }

  if ( !(ssl_sock = calloc(1, sizeof(*ssl_sock))) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC, "calloc(ssl_sock) fails: %s", strerror(errno));
    goto end;
  }

  if ( ssl_ctx && !(ssl_sock->ssl = co_ssl_new(ssl_ctx, cc)) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC, "co_ssl_new() fails: %s", strerror(errno));
    goto end;
  }

  ssl_sock->cc = cc;
  fok = true;

end:

  if ( !fok && ssl_sock ) {
    co_ssl_free(&ssl_sock->ssl);
    free(ssl_sock);
    ssl_sock = NULL;
  }

  return ssl_sock;
}

void co_ssl_socket_free(co_ssl_socket ** ssl_sock)
{
  if ( ssl_sock && *ssl_sock ) {
    co_ssl_free(&(*ssl_sock)->ssl);
    free(*ssl_sock);
    *ssl_sock = NULL;
  }
}

void co_ssl_socket_close(co_ssl_socket ** ssl_sock, bool abort_conn)
{
  if ( ssl_sock && *ssl_sock ) {
    co_socket_close(&(*ssl_sock)->cc, abort_conn);
    co_ssl_socket_free(ssl_sock);
  }
}


void co_ssl_socket_set_send_timeout(co_ssl_socket * ssl_sock, int msec)
{
  return co_socket_set_send_tmout(ssl_sock->cc, msec);
}

void co_ssl_socket_set_recv_timeout(co_ssl_socket * ssl_sock, int msec)
{
  return co_socket_set_recv_tmout(ssl_sock->cc, msec);
}

ssize_t co_ssl_socket_send(co_ssl_socket * ssl_sock, const void * buf, size_t size)
{
  ssize_t bytes_sent;

  if ( ssl_sock->ssl ) {
    bytes_sent = SSL_write(ssl_sock->ssl, buf, size);
  }
  else if ( ssl_sock->cc ) {
    bytes_sent = co_socket_send(ssl_sock->cc, buf, size, 0);
  }
  else {
    bytes_sent = -1;
    errno = EBADF;
  }

  return bytes_sent;
}

ssize_t co_ssl_socket_recv(co_ssl_socket * ssl_sock, void * buf, size_t size)
{
  ssize_t bytes_received;

  if ( ssl_sock->ssl ) {
    bytes_received = SSL_read(ssl_sock->ssl, buf, size);
  }
  else if ( ssl_sock->cc ) {
    bytes_received = co_socket_recv(ssl_sock->cc, buf, size, 0);
  }
  else {
    bytes_received = -1;
    errno = EBADF;
  }

  return bytes_received;
}


co_ssl_socket * co_ssl_socket_accept(co_socket ** cc, SSL_CTX * ssl_ctx)
{
  co_ssl_socket * ssl_sock = NULL;

  if ( !(ssl_sock = co_ssl_socket_new(*cc, ssl_ctx)) ) {
    CF_SSL_ERR(CF_SSL_ERR_CUTTLE, "co_ssl_socket_new() fails");
  }
  else if ( SSL_accept(ssl_sock->ssl) != 1 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_accept() fails");
    co_ssl_socket_free(&ssl_sock);
  }
  else {
    *cc = NULL;
  }

  return ssl_sock;
}


co_socket * co_ssl_listen(const struct sockaddr * addrs, int sock_type, int proto)
{
  co_socket * cc = NULL;
  int so = -1;

  bool fok = false;

  if ( !sock_type ) {
    sock_type = SOCK_STREAM;
  }

  if ( !proto ) {
    proto = IPPROTO_TCP;
  }

  if ( (so = socket(addrs->sa_family, sock_type, proto)) == -1 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "socket(sa_family=%d sock_type=%d proto=%d) fails: %s", addrs->sa_family, sock_type,
        proto, strerror(errno));
    goto end;
  }

  so_set_reuse_addrs(so, 1);

  if ( !so_set_non_blocking(so, 1) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "so_set_non_blocking() fails: %s", strerror(errno));
    goto end;
  }

  if ( bind(so, addrs, so_get_addrlen(addrs)) == -1 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "bind() fails: %s", strerror(errno));
    goto end;
  }

  if ( listen(so, SOMAXCONN) == -1 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "listen() fails: %s", strerror(errno));
    goto end;
  }

  if ( !(cc = co_socket_new(so)) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_new() fails: %s", strerror(errno));
    goto end;
  }

  fok = true;

end: ;

  if ( !fok ) {

    int errno_backup = errno;

    if ( cc ) {
      co_socket_close(&cc, true);
    }

    errno = errno_backup;
  }

  return cc;

}


co_ssl_socket * co_ssl_connect(const struct sockaddr * addrs, const struct co_ssl_connect_opts * opts)
{
  co_ssl_socket * ssl_sock = NULL;
  co_socket * cc = NULL;
  int so = -1;

  SSL_CTX * ssl_ctx = opts ? opts->ssl_ctx : NULL;
  int sock_type = opts && opts->sock_type ? opts->sock_type : SOCK_STREAM;
  int proto = opts && opts->proto ? opts->proto : IPPROTO_TCP;
  int tmout = opts && opts->tmout ? opts->tmout : 15 * 1000;

  int status;

  bool fok = false;

  if ( (so = socket(addrs->sa_family, sock_type, proto)) == -1 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "socket(sa_family=%d sock_type=%d proto=%d) fails: %s", addrs->sa_family,
        sock_type, proto, strerror(errno));
    goto end;
  }

  if ( !(cc = co_socket_connect_new(so, addrs, so_get_addrlen(addrs), tmout)) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_connect_new(so=%d) fails: %s", so, strerror(errno));
    goto end;
  }

  if ( !(ssl_sock = co_ssl_socket_new(cc, ssl_ctx)) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_ssl_socket_new() fails");
    goto end;
  }

  if ( ssl_sock->ssl ) {

    co_socket_set_sndrcv_tmouts(cc, tmout, tmout);

    if ( (status = SSL_connect(ssl_sock->ssl)) != 1 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_connect() fails: status=%d %s", status,
          cf_get_ssl_error_string(ssl_sock->ssl, status));
      goto end;
    }
  }

  fok = true;

end:

  if ( !fok ) {

    int errno_backup = errno;

    if ( ssl_sock ) {
      co_ssl_socket_close(&ssl_sock, true);
    }
    else if ( so != -1 ) {
      so_close(so, true);
    }

    errno = errno_backup;
  }

  return ssl_sock;

}


co_ssl_socket * co_ssl_server_connect(const char * address, uint16_t port, const struct co_ssl_connect_opts * opts)
{
  co_ssl_socket * ssl_sock = NULL;
  int status;

  struct addrinfo * ai = NULL;
  const struct addrinfo addrshints = {
    .ai_family = PF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_V4MAPPED
  };

  int tmo_ms = opts && opts->tmout ? opts->tmout : 15 * 1000;

  bool fok = false;

  if ( !port && ai->ai_addr->sa_family != AF_UNIX ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Destination port not specified");
    errno = EDESTADDRREQ;
    goto end;
  }

  if ( (status = co_resolve(address, &ai, &addrshints, tmo_ms > 0 ? tmo_ms / 1000 : 15 * 1000)) ) {
    CF_SSL_ERR(CF_SSL_ERR_CUTTLE, "co_resolve() fails: status=%d %s", status, co_resolve_strerror(status));
    goto end;
  }

  if ( ai->ai_addr->sa_family == AF_INET ) {
    ((struct sockaddr_in*) ai->ai_addr)->sin_port = htons(port);
  }
  else if ( ai->ai_addr->sa_family == AF_INET6 ) {
    ((struct sockaddr_in6*) ai->ai_addr)->sin6_port = htons(port);
  }

  if ( !(ssl_sock = co_ssl_connect(ai->ai_addr, opts)) ) {
    goto end;
  }

  fok = true;

end : ;

  if ( !fok ) {
    co_ssl_socket_close(&ssl_sock, true);
  }

  return ssl_sock;

}


