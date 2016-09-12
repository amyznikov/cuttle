/*
 * co-tcp-server.c
 *
 *  Created on: Sep 9, 2016
 *      Author: amyznikov
 */

#include <cuttle/cothread/ssl-server.h>
#include <cuttle/debug.h>
#include <cuttle/ccarray.h>
#include <malloc.h>
#include <errno.h>
#include <string.h>


#define CO_SERVER_LISTENING_THREAD_STACK_SIZE         (256*1024)

static void co_ssl_server_listening_thread(void * arg)
{
  co_ssl_listening_port * sslp = arg;

  co_socket * listening_sock = sslp->listening_sock;
  co_socket * accepted_sock = NULL;

  CF_DEBUG("Started");


  while ( 42 ) {

    bool fok = false;

    if ( !(accepted_sock = co_socket_accept_new(listening_sock, NULL, 0)) ) {
      CF_CRITICAL("co_ssl_socket_accept() fails");
    }
    else if ( !sslp->onaccept(sslp, accepted_sock) ) {
      CF_CRITICAL("onaccept() fails");
    }
    else {
      fok = true;
    }

    if ( !fok ) {
      co_socket_close(&accepted_sock, true);
    }
  }

  CF_DEBUG("Finished");
}





bool co_ssl_server_init(struct co_ssl_server * sslsrv, const co_ssl_server_opts * opts)
{
  int max_nb_ports = opts && opts->max_nb_ports > 0 ? opts->max_nb_ports : 8;

  bool fok = false;

  if ( !ccarray_init(&sslsrv->ssl_ports, max_nb_ports, sizeof(co_ssl_listening_port*)) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC, "ccarray_init(ssl_ports: max_nb_ports=%d) fails: %s", max_nb_ports, strerror(errno));
    goto end;
  }

  if ( opts ) {
    sslsrv->ssl_ctx = opts->ssl_ctx;
  }

  fok = true;

end :

  return fok;
}

void co_ssl_server_cleanup(struct co_ssl_server * sslsrv)
{
  for ( size_t i = 0, n = ccarray_size(&sslsrv->ssl_ports); i < n; ++i ) {
    co_ssl_listening_port_release(ccarray_peek(&sslsrv->ssl_ports, i));
  }
  ccarray_cleanup(&sslsrv->ssl_ports);
}


co_ssl_server * co_ssl_server_new(const co_ssl_server_opts * opts)
{
  (void)(opts);

  co_ssl_server * sslsrv = NULL;
  size_t obj_size = sizeof(*sslsrv);

  bool fok = false;

  if ( opts && opts->extra_obj_size > 0 ) {
    obj_size += opts->extra_obj_size;
  }

  if ( !(sslsrv = calloc(1, obj_size)) ) {
    CF_SSL_ERR(CF_SSL_ERR_MALLOC, "calloc(sslsrv: %zu bytes) fails: %s", obj_size, strerror(errno));
    goto end;
  }

  if ( !co_ssl_server_init(sslsrv, opts) ) {
    goto end;
  }

  fok = true;

end:

  if ( !fok && sslsrv ) {
    co_ssl_server_destroy(&sslsrv);
  }

  return sslsrv;
}

void co_ssl_server_destroy(co_ssl_server ** sslsrv)
{
  if ( sslsrv && *sslsrv ) {
    co_ssl_server_cleanup(*sslsrv);
    free(*sslsrv);
    *sslsrv = NULL;
  }
}


bool co_ssl_server_add_port(co_ssl_server * ssrv, co_ssl_listening_port * sslp)
{
  bool fok = false;

  if ( ccarray_size(&ssrv->ssl_ports) >= ccarray_capacity(&ssrv->ssl_ports) ) {
    errno = ENOBUFS;
    goto end;
  }

  ccarray_ppush_back(&ssrv->ssl_ports, sslp);
  fok = true;

end:

  return fok;
}

co_ssl_listening_port * co_ssl_server_add_new_port(co_ssl_server * sslsrv, const co_ssl_listening_port_opts * opts)
{
  co_ssl_listening_port * sslp = NULL;

  bool fok = false;

  if ( ccarray_size(&sslsrv->ssl_ports) >= ccarray_capacity(&sslsrv->ssl_ports) ) {
    errno = ENOBUFS;
    goto end;
  }

  if ( !(sslp = co_ssl_listening_port_new(opts)) ) {
    goto end;
  }

  fok = co_ssl_server_add_port(sslsrv, sslp);

end:

  if ( !fok ) {
    co_ssl_listening_port_release(&sslp);
  }

  return sslp;
}


bool co_ssl_server_start(co_ssl_server * ssrv)
{
  co_ssl_listening_port * sslp;
  bool fok = true;

  for ( size_t i = 0, n = ccarray_size(&ssrv->ssl_ports); i < n; ++i ) {

    sslp = ccarray_ppeek(&ssrv->ssl_ports, i);

    if ( !(sslp->listening_sock = co_ssl_listen(&sslp->listen_address.sa, sslp->sock_type, sslp->proto)) ) {
      CF_FATAL("co_ssl_listen() fails");
      fok = false;
      break;
    }

    if ( !co_schedule(co_ssl_server_listening_thread, sslp, CO_SERVER_LISTENING_THREAD_STACK_SIZE) ) {
      CF_FATAL("co_schedule(co_ssl_server_listening_thread) fails: %s", strerror(errno));
      co_socket_close(&sslp->listening_sock, false);
      fok = false;
      break;
    }
  }

  return fok;
}

bool co_ssl_server_stop(co_ssl_server * ssrv)
{
  (void)(ssrv);
  return false;
}

