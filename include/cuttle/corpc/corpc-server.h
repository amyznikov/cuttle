/*
 * corpc-server.h
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

// #pragma once

#ifndef __cuttle_corpc_server_h__
#define __cuttle_corpc_server_h__

#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <cuttle/corpc/listening-port.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef
struct corpc_server
  corpc_server;

typedef
struct corpc_server_opts {
  SSL_CTX * ssl_ctx;
  int max_nb_ports;
} corpc_server_opts;


corpc_server * corpc_server_new(const corpc_server_opts * opts);
void corpc_server_destroy(corpc_server ** cosrv);
bool corpc_server_add_port(corpc_server * cosrv, const corpc_listening_port_opts * opts);
bool corpc_server_start(corpc_server * cosrv);


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_server_h__*/
