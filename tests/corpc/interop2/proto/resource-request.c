/*
 * resource-request.c
 *
 *  Created on: Feb 9, 2018
 *      Author: amyznikov
 */

#include "resource-request.h"


bool corpc_pack_resource_request(const struct resource_request * request, corpc_msg * msg)
{
  msg->data = strdup(request->resource_id);
  msg->size = strlen(msg->data) + 1;
  return true;
}

bool corpc_unpack_resource_request(const corpc_msg * msg, struct resource_request * request)
{
  request->resource_id = strdup(msg->data);
  return true;
}

bool corpc_pack_resource_responce(const struct resource_request_responce * responce, corpc_msg * msg)
{
  msg->data = strdup(responce->resource_data);
  msg->size = strlen(msg->data) + 1;
  return true;
}

bool corpc_unpack_resource_responce(const corpc_msg * msg, struct resource_request_responce * responce)
{
  responce->resource_data = strdup(msg->data);
  return true;
}


bool corpc_stream_write_resource_request(corpc_stream * st, const struct resource_request * request)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_resource_request(request, &msg) ) {
    fok = corpc_stream_write(st, msg.data, msg.size);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_resource_request(corpc_stream * st, struct resource_request * request)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( (msg.size = corpc_stream_read(st, &msg.data)) > 0 ) {
    fok = corpc_unpack_resource_request(&msg, request);
  }

  corpc_msg_clean(&msg);

  return fok;
}


bool corpc_stream_write_resource_request_responce(corpc_stream * st, const struct resource_request_responce * responce)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_resource_responce(responce, &msg) ) {
    fok = corpc_stream_write(st, msg.data, msg.size);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_resource_request_responce(corpc_stream * st, struct resource_request_responce * responce)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( (msg.size = corpc_stream_read(st, &msg.data)) > 0 ) {
    fok = corpc_unpack_resource_responce(&msg, responce);
  }

  corpc_msg_clean(&msg);

  return fok;
}

