/*
 * auth.h
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_tests_corpc_proto_auth_h__
#define __cuttle_tests_corpc_proto_auth_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <cuttle/corpc/corpc-msg.h>
#include <cuttle/corpc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef
struct auth_request {
  char * iam;
} auth_request;

typedef
struct auth_cookie {
  uint8_t cookie[256];
} auth_cookie;

typedef
struct auth_cookie_sign {
  uint8_t sign[256];
} auth_cookie_sign;

typedef
struct auth_responce {
  uint8_t ticket[256];
} auth_responce;


bool corpc_pack_auth_request(const struct auth_request * auth_request, corpc_msg * msg);
bool corpc_unpack_auth_request(const corpc_msg * msg, struct auth_request * auth_request);

bool corpc_pack_auth_cookie(const struct auth_cookie * auth_cookie, corpc_msg * msg);
bool corpc_unpack_auth_cookie(const corpc_msg * msg, struct auth_cookie * auth_cookie);

bool corpc_pack_auth_cookie_sign(const struct auth_cookie_sign * auth_cookie_sign, corpc_msg * msg);
bool corpc_unpack_auth_cookie_sign(const corpc_msg * msg, struct auth_cookie_sign * auth_cookie_sign);

bool corpc_pack_auth_responce(const struct auth_responce * auth_responce, corpc_msg * msg);
bool corpc_unpack_auth_responce(const corpc_msg * msg, struct auth_responce * auth_responce);


bool corpc_stream_write_auth_request(corpc_stream * st, const struct auth_request * auth_request);
bool corpc_stream_read_auth_request(corpc_stream * st, struct auth_request * auth_request);

bool corpc_stream_write_auth_cookie(corpc_stream * st, const struct auth_cookie * auth_cookie);
bool corpc_stream_read_auth_cookie(corpc_stream * st, struct auth_cookie * auth_cookie);

bool corpc_stream_write_auth_cookie_sign(corpc_stream * st, const struct auth_cookie_sign * auth_cookie_sign);
bool corpc_stream_read_auth_cookie_sign(corpc_stream * st, struct auth_cookie_sign * auth_cookie_sign);

bool corpc_stream_write_auth_responce(corpc_stream * st, const struct auth_responce * auth_responce);
bool corpc_stream_read_auth_responce(corpc_stream * st, struct auth_responce * auth_responce);


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_tests_corpc_proto_auth_h__ */
