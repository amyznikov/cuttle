/*
 * corpc-proto.h
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#pragma once

#ifndef __cuttle_corpc_proto_h__
#define __cuttle_corpc_proto_h__

#include <stddef.h>
#include <inttypes.h>
#include <stdbool.h>
#include <sys/types.h>
#include <cuttle/cothread/ssl.h>

#ifdef __cplusplus
extern "C" {
#endif


enum {
  co_msg_create_stream_req = 1,
  co_msg_create_stream_resp = 2,
  co_msg_close_stream_req = 3,
  co_msg_data = 4,
  co_msg_data_ack = 5,
};


typedef
enum create_stream_responce_code {
  create_stream_responce_ok = 0,
  create_stream_responce_no_stream_resources = 1,
  create_stream_responce_no_service = 2,
  create_stream_responce_no_method = 3,
  create_stream_responce_internal_error = 4,
  create_stream_responce_protocol_error = 5,
} create_stream_responce_code;

const char * create_stream_responce_status_string(
    enum create_stream_responce_code code);



#pragma pack(push, 1)

typedef
struct comsghdr {
  uint32_t crc;
  uint16_t code;
  uint16_t sid;
  uint16_t did;
  uint16_t pldsize; // payload size
} comsghdr;


#define CORPC_MAX_MSG_SIZE        (64*1024)
#define CORPC_MAX_PAYLOAD_SIZE    ((CORPC_MAX_MSG_SIZE) - sizeof(struct comsghdr))




typedef
struct comsg_create_stream_request {
  struct comsghdr hdr;
  struct {
    uint16_t rwnd;
    uint16_t service_name_length;
    uint16_t method_name_length;
    uint8_t  pack[];
  } details;
} comsg_create_stream_request;

typedef
struct comsg_create_stream_responce {
  struct comsghdr hdr;
  struct {
    uint16_t status;
    uint16_t rwnd;
  } details;
} comsg_create_stream_responce;







typedef
struct comsg_close_stream {
  struct comsghdr hdr;
  struct {
    uint16_t status;
  } details;
} comsg_close_stream;


typedef
struct comsg_data {
  struct comsghdr hdr;
  struct {
    uint8_t bits[CORPC_MAX_PAYLOAD_SIZE];
  } details;
} comsg_data;

typedef
struct comsg_data_ack {
  struct comsghdr hdr;
} comsg_data_ack;


//

typedef
struct comsg {
  union {
    comsghdr hdr;
    comsg_create_stream_request create_stream_request;
    comsg_create_stream_responce create_stream_responce;
    comsg_close_stream close_stream;
    comsg_data data;
    comsg_data_ack data_ack;
  };
} comsg;


#pragma pack(pop)


bool corpc_proto_recv_msg(co_ssl_socket * ssl_sock, comsg * msgp);
bool corpc_proto_send_create_stream_request(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t rwnd, const char * service, const char * method);
bool corpc_proto_send_create_stream_responce(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, uint16_t rwnd, uint16_t status);
bool corpc_proto_send_close_stream(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, uint16_t status);
bool corpc_proto_send_data(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, const void * data, size_t size);
bool corpc_proto_send_data_ack(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did);




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_proto_h__*/
