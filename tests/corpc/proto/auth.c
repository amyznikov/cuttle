/*
 * auth.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

#include "auth.h"


bool corpc_pack_auth_request(const struct auth_request * auth_request, corpc_msg * msg)
{
  return false;
}

bool corpc_unpack_auth_request(const corpc_msg * msg, struct auth_request * auth_request)
{
  return false;
}



bool corpc_pack_auth_cookie(const struct auth_cookie * auth_cookie, corpc_msg * msg)
{
  return false;
}

bool corpc_unpack_auth_cookie(const corpc_msg * msg, struct auth_cookie * auth_cookie)
{
  return false;
}



bool corpc_pack_auth_cookie_sign(const struct auth_cookie_sign * auth_cookie_sign, corpc_msg * msg)
{
  return false;
}

bool corpc_unpack_auth_cookie_sign(const corpc_msg * msg, struct auth_cookie_sign * auth_cookie_sign)
{
  return false;
}

bool corpc_pack_auth_responce(const struct auth_responce * auth_responce, corpc_msg * msg)
{
  return false;
}

bool corpc_unpack_auth_responce(const corpc_msg * msg, struct auth_responce * auth_responce)
{
  return false;
}



bool corpc_stream_write_auth_request(corpc_stream * st, const struct auth_request * auth_request)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_request(auth_request, &msg) ) {
    fok = corpc_stream_write(st, &msg);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_request(corpc_stream * st, struct auth_request * auth_request)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_stream_read(st, &msg) ) {
    fok = corpc_unpack_auth_request(&msg, auth_request);
  }

  return fok;
}


bool corpc_stream_write_auth_cookie(corpc_stream * st, const struct auth_cookie * auth_cookie)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_cookie(auth_cookie, &msg) ) {
    fok = corpc_stream_write(st, &msg);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_cookie(corpc_stream * st, struct auth_cookie * auth_cookie)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_stream_read(st, &msg) ) {
    fok = corpc_unpack_auth_cookie(&msg, auth_cookie);
  }

  return fok;
}


bool corpc_stream_write_auth_cookie_sign(corpc_stream * st, const struct auth_cookie_sign * auth_cookie_sign)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_cookie_sign(auth_cookie_sign, &msg) ) {
    fok = corpc_stream_write(st, &msg);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_cookie_sign(corpc_stream * st, struct auth_cookie_sign * auth_cookie_sign)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_stream_read(st, &msg) ) {
    fok = corpc_unpack_auth_cookie_sign(&msg, auth_cookie_sign);
  }

  return fok;
}


bool corpc_stream_write_auth_responce(corpc_stream * st, const struct auth_responce * auth_responce)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_pack_auth_responce(auth_responce, &msg) ) {
    fok = corpc_stream_write(st, &msg);
  }

  corpc_msg_clean(&msg);

  return fok;
}

bool corpc_stream_read_auth_responce(corpc_stream * st, struct auth_responce * auth_responce)
{
  corpc_msg msg;
  bool fok = false;

  corpc_msg_init(&msg);

  if ( corpc_stream_read(st, &msg) ) {
    fok = corpc_unpack_auth_responce(&msg, auth_responce);
  }

  return fok;
}




