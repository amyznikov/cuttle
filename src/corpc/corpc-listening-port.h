/*
 * listening-port.h
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_src_corpc_listening_port_h__
#define __cuttle_src_corpc_listening_port_h__

#include <cuttle/corpc/listening-port.h>
#include <cuttle/cothread/ssl-listening-port.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef
struct corpc_listening_port {
  struct co_ssl_listening_port base;
  const struct corpc_service ** services;
  bool (*onaccepted)(corpc_channel * channel);
  void (*ondisconnected)(corpc_channel * channel);
} corpc_listening_port;


corpc_listening_port * corpc_listening_port_new(const struct corpc_listening_port_opts * opts);
void corpc_listening_port_release(struct corpc_listening_port ** clp);
void * corpc_get_listening_port_cookie(const struct corpc_listening_port * clp);

bool corpc_listening_port_init(struct corpc_listening_port * cp, const struct corpc_listening_port_opts * opts);
void corpc_listening_port_cleanup(struct corpc_listening_port * cp);
void * corpc_get_listening_port_cookie(const struct corpc_listening_port * clp);



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_listening_port_h__ */
