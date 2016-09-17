/*
 * co-ssl-port.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */


#include <cuttle/debug.h>
#include <cuttle/cothread/ssl-listening-port.h>
#include <malloc.h>
#include <errno.h>

#define CO_SERVER_LISTENING_THREAD_STACK_SIZE         (64*1024)



static void co_ssl_listening_thread(void * arg)
{
  co_ssl_listening_port * sslp = arg;

  co_socket * listening_sock = sslp->listening_sock;
  co_socket * accepted_sock = NULL;
  co_ssl_socket * ssl_sock = NULL;

  CF_DEBUG("Started");


  while ( 42 ) {

    if ( !(accepted_sock = co_socket_accept_new(listening_sock, NULL, 0)) ) {
      CF_CRITICAL("co_ssl_socket_accept() fails: %s", strerror(errno));
    }
    else if ( !(ssl_sock = co_ssl_socket_attach(accepted_sock, sslp->ssl_ctx)) ) {
      CF_CRITICAL("co_ssl_socket_attach() fails: %s,  deleting co_socket", strerror(errno));
      co_socket_close(&accepted_sock, true);
    }
    else if ( !sslp->onaccept(sslp, ssl_sock) ) {
      CF_CRITICAL("onaccept() fails: deletig ssl_socket");
      co_ssl_socket_close(&ssl_sock, true);
    }
    else {
      CF_INFO("ACCEPTED");
    }
  }

  CF_DEBUG("Finished");
}



bool co_ssl_listening_port_init(struct co_ssl_listening_port * sslp, const struct co_ssl_listening_port_opts * opts)
{
  sslp->listen_address = opts->bind_address;
  sslp->ssl_ctx = opts->ssl_ctx;
  sslp->sock_type = opts->sock_type;
  sslp->proto = opts->proto;
  sslp->onaccept = opts->onaccept;
  sslp->cookie = opts->cookie;
  sslp->accepted_stack_size = opts->accepted_stack_size;

  return true;
}

void co_ssl_listening_port_cleanup(struct co_ssl_listening_port * sslp)
{
  memset(sslp, 0, sizeof(*sslp));
}

struct co_ssl_listening_port * co_ssl_listening_port_new(const struct co_ssl_listening_port_opts * opts)
{
  struct co_ssl_listening_port * sslp = NULL;

  size_t objsize = sizeof(*sslp);

  bool fok = true;

  if ( opts->extra_object_size > 0 ) {
    objsize += opts->extra_object_size;
  }

  if ( !(sslp = calloc(1, objsize)) ) {
    goto end;
  }

  if ( !co_ssl_listening_port_init(sslp, opts) ) {
    goto end;
  }

  fok = true;

end:

  if ( !fok ) {
    co_ssl_listening_port_release(&sslp);
  }

  return sslp;
}

void co_ssl_listening_port_release(struct co_ssl_listening_port ** sslpp)
{
  if ( sslpp && *sslpp ) {
    co_ssl_listening_port_cleanup(*sslpp);
    free(*sslpp);
    *sslpp = NULL;
  }
}

void * co_ssl_get_listening_port_cookie(const struct co_ssl_listening_port * sslp)
{
  return sslp->cookie;
}


bool co_ssl_listening_port_start_listen(struct co_ssl_listening_port * sslp)
{
  return co_schedule(co_ssl_listening_thread, sslp, CO_SERVER_LISTENING_THREAD_STACK_SIZE);
}

