/*
 * corpc-server.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */


#include <cuttle/corpc/server.h>
#include <cuttle/debug.h>
#include <cuttle/cothread/ssl-server.h>
#include "corpc-listening-port.h"

struct corpc_server {
  co_ssl_server base;
};



bool corpc_server_init(corpc_server * csslsrv, const corpc_server_opts * opts)
{
  return co_ssl_server_init(&csslsrv->base, &(struct co_ssl_server_opts ) {
        .ssl_ctx = opts->ssl_ctx,
        .max_nb_ports = opts->max_nb_ports,
        .extra_obj_size = 0
      });
}

void corpc_server_cleanup(corpc_server * csslsrv)
{
  co_ssl_server_cleanup(&csslsrv->base);
}


corpc_server * corpc_server_new(const corpc_server_opts * opts)
{
  return (corpc_server * )co_ssl_server_new(&(struct co_ssl_server_opts ) {
        .ssl_ctx = opts->ssl_ctx,
        .max_nb_ports = opts->max_nb_ports,
        .extra_obj_size = 0
      });
}


void corpc_server_destroy(corpc_server ** csslsrv)
{
  co_ssl_server_destroy((co_ssl_server **)csslsrv);
}

bool corpc_server_add_port(corpc_server * csslsrv, const corpc_listening_port_opts * opts)
{
  corpc_listening_port * clp = NULL;
  bool fok = false;

  if ( !(clp = corpc_listening_port_new(opts)) ) {
    goto end;
  }

  if ( !co_ssl_server_add_port(&csslsrv->base, &clp->base) ) {
    goto end;
  }

  fok = true;

end:

  if ( !fok ) {
    corpc_listening_port_release(&clp);
  }

  return fok;
}



bool corpc_server_start(corpc_server * csslrv)
{
  return co_ssl_server_start(&csslrv->base);
}
