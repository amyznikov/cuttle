/*
 * auth.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

#include "auth.h"


bool corpc_pack_auth_request(const struct auth_request * auth_request, corpc_msg * comsg)
{
  return false;
}

bool corpc_unpack_auth_request(struct auth_request * auth_request, const corpc_msg * comsg)
{
  return false;
}



bool corpc_pack_auth_cookie(const struct auth_cookie * auth_cookie, corpc_msg * comsg)
{
  return false;
}

bool corpc_unpack_auth_cookie(struct auth_cookie * auth_cookie, const corpc_msg * comsg)
{
  return false;
}



bool corpc_pack_auth_cookie_sign(const struct auth_cookie_sign * auth_cookie_sign, corpc_msg * comsg)
{
  return false;
}

bool corpc_unpack_auth_cookie_sign(struct auth_cookie_sign * auth_cookie_sign, const corpc_msg * comsg)
{
  return false;
}

bool corpc_pack_auth_responce(const struct auth_responce * auth_responce, corpc_msg * comsg)
{
  return false;
}

bool corpc_unpack_auth_responce(struct auth_responce * auth_responce, const corpc_msg * comsg)
{
  return false;
}





