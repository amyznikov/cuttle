/*
 * corpc-port.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */


#include "corpc-listening-port.h"
#include "corpc-channel.h"


#define tmpsslopts(opts) \
  fill_ssl_opts(opts, &(struct co_ssl_listening_port_opts){})



static bool corpc_listening_port_on_accept(co_ssl_listening_port * sslp, co_ssl_socket * accepted_sock)
{
  return corpc_channel_accept((corpc_listening_port * )sslp, accepted_sock) != NULL;
}






static const co_ssl_listening_port_opts * fill_ssl_opts(const struct corpc_listening_port_opts * opts, co_ssl_listening_port_opts * ssl_opts)
{
  ssl_opts->bind_address = opts->listen_address;
  ssl_opts->ssl_ctx = opts->ssl_ctx;
  ssl_opts->sock_type = SOCK_STREAM;
  ssl_opts->proto = IPPROTO_TCP;
  ssl_opts->onaccept = corpc_listening_port_on_accept;
  ssl_opts->cookie = NULL;
  ssl_opts->extra_object_size = sizeof(struct corpc_listening_port) - sizeof(struct co_ssl_listening_port);
  ssl_opts->accepted_stack_size = 0;
  return ssl_opts;
}



bool corpc_listening_port_init(struct corpc_listening_port * cp, const struct corpc_listening_port_opts * opts)
{
  if ( co_ssl_listening_port_init(&cp->base, tmpsslopts(opts)) ) {
    cp->services = opts->services;
    cp->nb_services = opts->nb_services;
    return true;
  }
  return false;
}

void corpc_listening_port_cleanup(struct corpc_listening_port * cp)
{
  co_ssl_listening_port_cleanup(&cp->base);
  cp->services = 0;
  cp->nb_services = 0;
}

struct corpc_listening_port * corpc_listening_port_new(const struct corpc_listening_port_opts * opts)
{
  corpc_listening_port * clp = NULL;

  if ( (clp = (corpc_listening_port *) co_ssl_listening_port_new(tmpsslopts(opts))) ) {
    clp->nb_services = opts->nb_services;
    clp->services = opts->services;
    clp->onaccepted = opts->onaccepted;
    clp->ondisconnected = opts->ondisconnected;
  }

  return clp;
}

void corpc_listening_port_release(struct corpc_listening_port ** clp)
{
  co_ssl_listening_port_release((co_ssl_listening_port **) clp);
}

void * corpc_get_listening_port_cookie(const struct corpc_listening_port * clp)
{
  return co_ssl_get_listening_port_cookie(&clp->base);
}
