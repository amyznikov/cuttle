/*
 * corpc/listening-port.h
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_corpc_listening_port_h__
#define __cuttle_corpc_listening_port_h__

#include <cuttle/corpc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef
struct corpc_listening_port_opts {

  sockaddr_type listen_address;
  SSL_CTX * ssl_ctx;

  const struct corpc_service **
    services;

  struct so_keepalive_opts
    keep_alive;

  bool (*onaccept)(const corpc_channel * channel);
  void (*onaccepted)(corpc_channel * channel);
  void (*ondisconnected)(corpc_channel * channel);

} corpc_listening_port_opts;



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_listening_port_h__ */
