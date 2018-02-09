/*
 * resource-request.h
 *
 *  Created on: Feb 9, 2018
 *      Author: amyznikov
 */

#pragma once

#ifndef __resource_request_h__
#define __resource_request_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <cuttle/corpc/channel.h>
#include "corpc-msg.h"


#ifdef __cplusplus
extern "C" {
#endif


typedef
struct resource_request {
  char * resource_id;
} resource_request;


typedef
struct resource_request_responce {
  char * resource_data;
} resource_request_responce;


bool corpc_pack_resource_request(const struct resource_request * resource_request, corpc_msg * msg);
bool corpc_unpack_resource_request(const corpc_msg * msg, struct resource_request * resource_request);

bool corpc_pack_resource_responce(const struct resource_request_responce * responce, corpc_msg * msg);
bool corpc_unpack_resource_responce(const corpc_msg * msg, struct resource_request_responce * responce);

bool corpc_stream_write_resource_request(corpc_stream * st, const struct resource_request * request);
bool corpc_stream_read_resource_request(corpc_stream * st, struct resource_request * request);

bool corpc_stream_write_resource_request_responce(corpc_stream * st, const struct resource_request_responce * responce);
bool corpc_stream_read_resource_request_responce(corpc_stream * st, struct resource_request_responce * responce);

#ifdef __cplusplus
}
#endif

#endif /* __resource_request_h__ */
