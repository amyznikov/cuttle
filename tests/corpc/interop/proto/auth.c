/*
 * auth.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

#include "auth.h"


bool corpc_pack_auth_request(const struct auth_request * auth_request, corpc_msg * msg)
{
  msg->data = strdup(auth_request->text);
  msg->size = strlen(msg->data) + 1;
  return true;
}

bool corpc_unpack_auth_request(const corpc_msg * msg, struct auth_request * auth_request)
{
  auth_request->text = strdup(msg->data);
  return true;
}



bool corpc_pack_auth_cookie(const struct auth_cookie * auth_cookie, corpc_msg * msg)
{
  msg->data = strdup(auth_cookie->text);
  msg->size = strlen(msg->data) + 1;
  return true;
}

bool corpc_unpack_auth_cookie(const corpc_msg * msg, struct auth_cookie * auth_cookie)
{
  auth_cookie->text = strdup(msg->data);
  return true;
}



bool corpc_pack_auth_cookie_sign(const struct auth_cookie_sign * auth_cookie_sign, corpc_msg * msg)
{
  msg->data = strdup(auth_cookie_sign->text);
  msg->size = strlen(msg->data) + 1;
  return true;
}

bool corpc_unpack_auth_cookie_sign(const corpc_msg * msg, struct auth_cookie_sign * auth_cookie_sign)
{
  auth_cookie_sign->text = strdup(msg->data);
  return true;
}

bool corpc_pack_auth_responce(const struct auth_responce * auth_responce, corpc_msg * msg)
{
  msg->data = strdup(auth_responce->text);
  msg->size = strlen(msg->data) + 1;
  return true;
}

bool corpc_unpack_auth_responce(const corpc_msg * msg, struct auth_responce * auth_responce)
{
  auth_responce->text = strdup(msg->data);
  return true;
}



bool corpc_stream_write_auth_request(corpc_stream * st, const struct auth_request * auth_request)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_request(auth_request, &msg) ) {
    fok = corpc_stream_write(st, msg.data, msg.size);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_request(corpc_stream * st, struct auth_request * auth_request)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( (msg.size = corpc_stream_read(st, &msg.data)) > 0 ) {
    fok = corpc_unpack_auth_request(&msg, auth_request);
  }

  corpc_msg_clean(&msg);

  return fok;
}


bool corpc_stream_write_auth_cookie(corpc_stream * st, const struct auth_cookie * auth_cookie)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_cookie(auth_cookie, &msg) ) {
    fok = corpc_stream_write(st, msg.data, msg.size);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_cookie(corpc_stream * st, struct auth_cookie * auth_cookie)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( (msg.size = corpc_stream_read(st, &msg.data)) > 0 ) {
    fok = corpc_unpack_auth_cookie(&msg, auth_cookie);
  }

  corpc_msg_clean(&msg);

  return fok;
}


bool corpc_stream_write_auth_cookie_sign(corpc_stream * st, const struct auth_cookie_sign * auth_cookie_sign)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_cookie_sign(auth_cookie_sign, &msg) ) {
    fok = corpc_stream_write(st, msg.data, msg.size);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_cookie_sign(corpc_stream * st, struct auth_cookie_sign * auth_cookie_sign)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( (msg.size = corpc_stream_read(st, &msg.data)) > 0 ) {
    fok = corpc_unpack_auth_cookie_sign(&msg, auth_cookie_sign);
  }

  corpc_msg_clean(&msg);

  return fok;
}


bool corpc_stream_write_auth_responce(corpc_stream * st, const struct auth_responce * auth_responce)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_responce(auth_responce, &msg) ) {
    fok = corpc_stream_write(st, msg.data, msg.size);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_responce(corpc_stream * st, struct auth_responce * auth_responce)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( (msg.size = corpc_stream_read(st, &msg.data)) > 0 ) {
    fok = corpc_unpack_auth_responce(&msg, auth_responce);
  }

  corpc_msg_clean(&msg);

  return fok;
}




