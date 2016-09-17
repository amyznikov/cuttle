/*
 * corpc-proto.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */


#include <cuttle/debug.h>
#include <cuttle/hash/crc32.h>
#include <arpa/inet.h>
#include <alloca.h>
#include "corpc-proto.h"

#define SEND_DEBUG(...)
  //  CF_NOTICE(__VA_ARGS__)
#define RECV_DEBUG(...)
  //  CF_NOTICE(__VA_ARGS__)

#define co_proto_recv_chunk(ssl_sock, data) \
    co_proto_read(ssl_sock,(data),sizeof(*(data)))

static bool co_proto_read(co_ssl_socket * ssl_sock, void * p, size_t size)
{
  return co_ssl_socket_recv(ssl_sock, p,size) == (ssize_t)size;
}

static uint32_t crc_begin() {
  return cf_crc32_begin();
}

static uint32_t crc_update(uint32_t crc, const void * data, size_t size) {
  return cf_crc32_update(crc, data, size);
}

static uint32_t crc_final(uint32_t crc) {
  return cf_crc32_finalize(crc);
}

static uint32_t calc_crc(const comsghdr * hdr, size_t msgsize)
{
  return crc_final(crc_update(crc_begin(), (const uint8_t*) hdr + sizeof(hdr->crc), msgsize - sizeof(hdr->crc)));
}

static void set_crc(comsghdr * hdr, size_t msgsize)
{
  hdr->crc = calc_crc(hdr, msgsize);
}


static void ntohdr(comsghdr * msgh)
{
  msgh->crc = ntohl(msgh->crc);
  msgh->code = ntohs(msgh->code);
  msgh->sid = ntohs(msgh->sid);
  msgh->did = ntohs(msgh->did);
  msgh->pldsize = ntohs(msgh->pldsize);
}

static void htondr(comsghdr * msgh)
{
  msgh->crc = htonl(msgh->crc);
  msgh->code = htons(msgh->code);
  msgh->sid = htons(msgh->sid);
  msgh->did = htons(msgh->did);
  msgh->pldsize = htons(msgh->pldsize);
}


const char * create_stream_responce_status_string(enum create_stream_responce_code code)
{
  static __thread char buf[64];

  switch ( code ) {
    case create_stream_responce_ok :
      return "OK";
    case create_stream_responce_no_stream_resources :
      return "no_stream_resources";
    case create_stream_responce_no_service :
      return "no_service";
    case create_stream_responce_no_method :
      return "no_method";
    case create_stream_responce_internal_error :
      return "internal_error";
    case create_stream_responce_protocol_error:
      return "protocol_error";
  }

  snprintf(buf, sizeof(buf) - 1, "ERROR %d", code);
  return buf;
}



bool corpc_proto_recv_msg(co_ssl_socket * ssl_sock, comsg * msgp)
{
  ssize_t size;
  bool fok = false;
  uint32_t crc_received, crc_actual;

  if ( !co_proto_recv_chunk(ssl_sock, &msgp->hdr) ) {
    CF_CRITICAL("co_proto_recv_chunk() fails");
    goto end;
  }

  ntohdr(&msgp->hdr);

  switch (msgp->hdr.code) {

    case co_msg_create_stream_req :
      RECV_DEBUG("recv: create_stream_req sid=%u did=%u", msgp->hdr.sid, msgp->hdr.did);

      if ( msgp->hdr.pldsize > CORPC_MAX_PAYLOAD_SIZE ) {
        CF_CRITICAL("msgp->hdr.size is too large: %u", msgp->hdr.pldsize);
        goto end;
      }

      if ( (size = co_proto_read(ssl_sock, &msgp->create_stream_request.details, msgp->hdr.pldsize)) <= 0 ) {
        CF_CRITICAL("co_proto_read() fails: size=%zd", size);
        goto end;
      }

      msgp->create_stream_request.details.rwnd = ntohs(
          msgp->create_stream_request.details.rwnd);
      msgp->create_stream_request.details.service_name_length = ntohs(
          msgp->create_stream_request.details.service_name_length);
      msgp->create_stream_request.details.method_name_length = ntohs(
          msgp->create_stream_request.details.method_name_length);

    break;




    case co_msg_create_stream_resp:
      RECV_DEBUG("recv: create_stream_resp sid=%u did=%u", msgp->hdr.sid, msgp->hdr.did);

      if ( msgp->hdr.pldsize != sizeof(msgp->create_stream_responce.details) ) {
        CF_CRITICAL("msgp->hdr.size is too large: %u. Expected %zu", msgp->hdr.pldsize, sizeof(msgp->create_stream_responce.details));
        goto end;
      }

      if ( !co_proto_recv_chunk(ssl_sock, &msgp->create_stream_responce.details) ) {
        CF_CRITICAL("co_proto_recv_chunk() fails");
        goto end;
      }

      msgp->create_stream_responce.details.status = ntohs(msgp->create_stream_responce.details.status);
      msgp->create_stream_responce.details.rwnd = ntohs(msgp->create_stream_responce.details.rwnd);

      break;




    case co_msg_close_stream_req:
      RECV_DEBUG("recv: close_stream_req sid=%u did=%u", msgp->hdr.sid, msgp->hdr.did);

      if ( msgp->hdr.pldsize != sizeof(msgp->close_stream.details) ) {
        CF_CRITICAL("msgp->hdr.size is too large: %u. Expected %zu", msgp->hdr.pldsize, sizeof(msgp->close_stream.details));
        goto end;
      }

      if ( !co_proto_recv_chunk(ssl_sock, &msgp->close_stream.details) ) {
        CF_CRITICAL("co_proto_recv_chunk() fails");
        goto end;
      }

      msgp->close_stream.details.status = ntohs(msgp->close_stream.details.status);
      break;




    case co_msg_data:
      RECV_DEBUG("recv: data sid=%u did=%u", msgp->hdr.sid, msgp->hdr.did);

      if ( msgp->hdr.pldsize > CORPC_MAX_PAYLOAD_SIZE ) {
        CF_CRITICAL("msgp->hdr.size is too large: %u", msgp->hdr.pldsize);
        goto end;
      }

      if ( (size = co_proto_read(ssl_sock, &msgp->data.details, msgp->hdr.pldsize)) <= 0 ) {
        CF_CRITICAL("co_proto_read() fails: size=%zd", size);
        goto end;
      }

    break;

    case co_msg_data_ack:
      RECV_DEBUG("recv: data_ack sid=%u did=%u", msgp->hdr.sid, msgp->hdr.did);

      if ( msgp->hdr.pldsize != 0 ) {
        CF_CRITICAL("msgp->hdr.size is too large: %u. Expected 0", msgp->hdr.pldsize);
        goto end;
      }

      break;

    default:
      CF_CRITICAL("Invalid msgp->hdr.code=%u", msgp->hdr.code);
      errno = EPROTO;
      goto end;
  }

  crc_received = msgp->hdr.crc;

  crc_actual = calc_crc(&msgp->hdr, sizeof(msgp->hdr) + msgp->hdr.pldsize);

  if ( crc_actual != crc_received ) {
    CF_CRITICAL("CRC NOT MATCH: crc_received=%u crc_actual=%u", crc_received, crc_actual);
    errno = EPROTO;
    goto end;
  }

  fok = true;

end:

  return fok;
}




bool corpc_proto_send_create_stream_request(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t rwnd, const char * service, const char * method)
{
  struct comsg_create_stream_request * msg;

  size_t service_name_length = strlen(service);
  size_t method_name_length = strlen(method);
  size_t payload_size = sizeof(msg->details) + service_name_length + method_name_length;
  size_t msgsize = offsetof(struct comsg_create_stream_request, details) + payload_size;

  msg = alloca(msgsize);
  msg->hdr.crc = 0;
  msg->hdr.code = co_msg_create_stream_req;
  msg->hdr.sid = sid;
  msg->hdr.did = 0;
  msg->hdr.pldsize = payload_size;
  msg->details.rwnd = rwnd;
  msg->details.service_name_length = service_name_length;
  msg->details.method_name_length = method_name_length;
  memcpy(msg->details.pack, service, service_name_length);
  memcpy(msg->details.pack + service_name_length, method, method_name_length);

  set_crc(&msg->hdr, msgsize);

  htondr(&msg->hdr);
  msg->details.rwnd = htons(msg->details.rwnd);
  msg->details.service_name_length = htons(service_name_length);
  msg->details.method_name_length = htons(method_name_length);

  SEND_DEBUG("send: create_stream_request sid=%u", sid);

  return (co_ssl_socket_send(ssl_sock, msg, msgsize) == (ssize_t)msgsize);
}


bool corpc_proto_send_create_stream_responce(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, uint16_t rwnd, uint16_t status)
{
  struct comsg_create_stream_responce msg = {
    .hdr = {
      .crc = 0,
      .code = co_msg_create_stream_resp,
      .pldsize = sizeof(msg.details),
      .sid = sid,
      .did = did,
    },
    .details = {
      .status = status,
      .rwnd = rwnd
    }
  };


  set_crc(&msg.hdr, sizeof(msg));

  htondr(&msg.hdr);
  msg.details.status = htons(msg.details.status);
  msg.details.rwnd = htons(msg.details.rwnd);


  SEND_DEBUG("send: create_stream_responce sid=%u did=%u", sid, did);
  return (co_ssl_socket_send(ssl_sock, &msg, sizeof(msg)) == sizeof(msg));
}

bool corpc_proto_send_close_stream(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, uint16_t status)
{
  struct comsg_close_stream msg = {
    .hdr = {
      .code = co_msg_close_stream_req,
      .pldsize = sizeof(msg.details),
      .sid = sid,
      .did = did,
    },
    .details = {
      .status = status
    }
  };

  set_crc(&msg.hdr, sizeof(msg));

  htondr(&msg.hdr);
  msg.details.status = htons(msg.details.status);

  SEND_DEBUG("send: close_stream");
  return (co_ssl_socket_send(ssl_sock, &msg, sizeof(msg)) == sizeof(msg));
}

bool corpc_proto_send_data_ack(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did)
{
  struct comsg_data_ack msg = {
    .hdr = {
      .code = co_msg_data_ack,
      .pldsize = 0,
      .sid = sid,
      .did = did,
    }
  };

  set_crc(&msg.hdr, sizeof(msg));

  htondr(&msg.hdr);

  SEND_DEBUG("send: data_ack sid=%u did=%u", sid, did);
  return (co_ssl_socket_send(ssl_sock, &msg, sizeof(msg)) == sizeof(msg));
}

bool corpc_proto_send_data(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, const void * data, size_t size)
{
  struct comsghdr msg = {
    .code = co_msg_data,
    .pldsize = size,
    .sid = sid,
    .did = did,
  };

  bool fok = false;

  msg.crc = crc_final(crc_update(crc_update(crc_begin(), (const uint8_t*) &msg + sizeof(msg.crc),
      sizeof(msg) - sizeof(msg.crc)), data, size));

  htondr(&msg);

  SEND_DEBUG("send: data sid=%u did=%u", sid, did);

  if ( co_ssl_socket_send(ssl_sock, &msg, sizeof(msg)) == sizeof(msg) ) {
    if ( co_ssl_socket_send(ssl_sock, data, size) == (ssize_t)size ) {
      fok = true;
    }
  }

  return fok;
}

