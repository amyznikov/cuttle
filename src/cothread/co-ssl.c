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


co_ssl_socket * co_ssl_socket_attach(co_socket * cc, SSL_CTX * ssl_ctx)
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

co_ssl_socket * co_ssl_socket_new(int af, int sock_type, int proto, SSL_CTX * ssl_ctx)
{
  co_socket * co_sock = NULL;
  co_ssl_socket * ssl_sock = NULL;

  if ( !(co_sock = co_socket_new(af, sock_type, proto) )) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_new() fails: %s", strerror(errno));
  }
  else if ( !(ssl_sock = co_ssl_socket_attach(co_sock, ssl_ctx)) ) {
    co_socket_close(&co_sock, false);
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


bool co_ssl_socket_set_send_timeout(co_ssl_socket * ssl_sock, int msec)
{
  bool fok = false;
  if ( !ssl_sock || !ssl_sock->cc ) {
    errno = EBADF;
  }
  else {
    fok = co_socket_set_send_tmout(ssl_sock->cc, msec);
  }
  return fok;
}

bool co_ssl_socket_set_recv_timeout(co_ssl_socket * ssl_sock, int msec)
{
  bool fok = false;
  if ( !ssl_sock || !ssl_sock->cc ) {
    errno = EBADF;
  }
  else {
    fok = co_socket_set_recv_tmout(ssl_sock->cc, msec);
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
  else if ( ssl_sock->cc ) {
    bytes_sent = co_socket_send(ssl_sock->cc, buf, size, 0);
  }
  else {
    errno = EBADF;
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
  else if ( ssl_sock->cc ) {
    if ( (bytes_received = co_socket_recv(ssl_sock->cc, buf, size, 0)) <= 0 ) {
      CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_recv() fails: %s", strerror(errno));
    }
  }
  else {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "invalid ssl_sock: ssl and cc are both NULL");
    errno = EBADF;
  }

  return bytes_received;
}



co_ssl_socket * co_ssl_socket_accept_new(co_socket ** cc, SSL_CTX * ssl_ctx)
{
  co_ssl_socket * ssl_sock = NULL;

  if ( !cc ) {
    errno = EINVAL;
  }
  else if ( !*cc ) {
    errno = EBADF;
  }
  else if ( !(ssl_sock = co_ssl_socket_attach(*cc, ssl_ctx)) ) {
    CF_SSL_ERR(CF_SSL_ERR_CUTTLE, "co_ssl_socket_new() fails");
  }
  else if ( ssl_sock->ssl && SSL_accept(ssl_sock->ssl) != 1 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_accept() fails");
    co_ssl_socket_free(&ssl_sock);
  }
  else {
    *cc = NULL;
  }

  return ssl_sock;
}

bool co_ssl_socket_accept(co_ssl_socket * ssl_sock)
{
  bool fok = false;

  if ( !ssl_sock ) {
    errno = EBADF;
  }
  else if ( ssl_sock->ssl && SSL_accept(ssl_sock->ssl) != 1 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_accept() fails");
  }
  else {
    fok = true;
  }

  return fok;
}

co_socket * co_ssl_listen(const struct sockaddr * addrs, int sock_type, int proto)
{
  co_socket * cc = NULL;
  int so = -1;

  bool fok = false;

  CF_DEBUG("ENTER");

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

  CF_DEBUG("C co_socket_new()");
  if ( !(cc = co_socket_attach(so)) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_new() fails: %s", strerror(errno));
    goto end;
  }
  CF_DEBUG("R co_socket_new()");

  fok = true;

end: ;

  if ( !fok ) {

    int errno_backup = errno;

    if ( cc ) {
      co_socket_close(&cc, true);
    }

    errno = errno_backup;
  }

  CF_DEBUG("LEAVE: cc=%p", cc);

  return cc;
}


bool co_ssl_connect(co_ssl_socket * ssl_sock, const struct sockaddr * addrs, int tmo)
{
  int status;
  bool fok = false;

  if ( !tmo ) {
    tmo = 15 * 1000;
  }

  co_socket_set_sndrcv_tmouts(ssl_sock->cc, tmo, tmo);

  if ( !(fok = co_socket_connect(ssl_sock->cc, addrs, so_get_addrlen(addrs), tmo)) ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "co_socket_connect() fails: %s", strerror(errno));
  }
  else if ( ssl_sock->ssl && (status = SSL_connect(ssl_sock->ssl)) != 1 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_connect() fails: status=%d %s", status,
        cf_get_ssl_error_string(ssl_sock->ssl, status));
  }
  else {
    fok = true;
  }

  return fok;
}

co_ssl_socket * co_ssl_connect_new(const struct sockaddr * addrs, const struct co_ssl_connect_opts * opts)
{
  co_ssl_socket * ssl_sock = NULL;

  if ( (ssl_sock = co_ssl_socket_new(addrs->sa_family, opts->sock_type, opts->proto, opts->ssl_ctx)) ) {
    if ( !co_ssl_connect(ssl_sock, addrs, opts->tmout) ) {
      co_ssl_socket_close(&ssl_sock, true);
    }
  }

  return ssl_sock;
}




bool co_ssl_server_connect(co_ssl_socket * ssl_sock, const char * address, uint16_t port, int tmo)
{
  struct addrinfo * ai = NULL;
  bool fok;

  if ( (fok = co_server_resolve(&ai, address, port, tmo)) ) {
    fok = co_ssl_connect(ssl_sock, ai->ai_addr, tmo);
  }

  if ( ai ) {
    freeaddrinfo(ai);
  }

  return fok;
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



