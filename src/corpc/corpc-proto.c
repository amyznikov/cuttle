/*
 * corpc-proto.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */


#include "corpc-proto.h"
#include <arpa/inet.h>
#include <cuttle/hash/crc32.h>


#define co_proto_recv_chunk(ssl_sock, data) \
    co_proto_read(ssl_sock, (data),sizeof(*(data)))

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
  msgh->size = ntohs(msgh->size);
  msgh->code = ntohs(msgh->code);
  // crc is always in network order
}


bool corpc_proto_recv_msg(co_ssl_socket * ssl_sock, comsg * msgp)
{
  ssize_t size;
  bool fok = false;
  uint32_t crc_received, crc_actual;

  if ( !co_proto_recv_chunk(ssl_sock, &msgp->hdr) ) {
    goto end;
  }

  ntohdr(&msgp->hdr);

  switch (msgp->hdr.code) {
    case co_msg_create_stream_req :
      if ( msgp->hdr.size != sizeof(msgp->create_stream_request.details) ) {
        goto end;
      }
      if ( !co_proto_recv_chunk(ssl_sock, &msgp->create_stream_request.details) ) {
        goto end;
      }
      msgp->create_stream_request.details.x = ntohs(msgp->create_stream_request.details.x);
    break;

    case co_msg_create_stream_resp:
      if ( msgp->hdr.size != sizeof(msgp->create_stream_responce.details) ) {
        goto end;
      }
      if ( !co_proto_recv_chunk(ssl_sock, &msgp->create_stream_responce.details) ) {
        goto end;
      }
      msgp->create_stream_responce.details.status = ntohs(msgp->create_stream_responce.details.status);
      break;

    case co_msg_close_stream_req:
      if ( msgp->hdr.size != sizeof(msgp->close_stream.details) ) {
        goto end;
      }
      if ( !co_proto_recv_chunk(ssl_sock, &msgp->close_stream.details) ) {
        goto end;
      }
      msgp->close_stream.details.status = ntohs(msgp->close_stream.details.status);
      break;

    case co_msg_data:
      if ( msgp->hdr.size > CORPC_MAX_PAYLOAD_SIZE ) {
        goto end;
      }
      if ( (size = co_proto_read(ssl_sock, msgp->data.details.bits, CORPC_MAX_PAYLOAD_SIZE)) <= 0 ) {
        goto end;
      }
    break;

    default:
      goto end;
  }

  crc_received = msgp->hdr.crc;
  crc_actual = calc_crc(&msgp->hdr, sizeof(msgp->hdr) + msgp->hdr.size);

  if ( crc_actual != crc_received ) {
    goto end;
  }

  fok = true;

end:

  return fok;
}

bool corpc_proto_send_create_stream_request(co_ssl_socket * ssl_sock, uint16_t sid)
{
  struct comsg_create_stream_request msg = {
    .hdr = {
      .code = htons(co_msg_create_stream_req),
      .size = htons(sizeof(msg.details)),
      .sid = htons(sid)
    },
    .details = {
      .x = 1
    }
  };

  set_crc(&msg.hdr, sizeof(msg));

  return co_ssl_socket_send(ssl_sock, &msg, sizeof(msg));
}


bool corpc_proto_send_create_stream_reply(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, uint16_t status)
{
  struct comsg_create_stream_responce msg = {
    .hdr = {
      .code = htons(co_msg_create_stream_resp),
      .size = htons(sizeof(msg.details)),
      .sid = htons(sid),
      .did = htons(did),
    },
    .details = {
      .status = htons(status)
    }
  };

  set_crc(&msg.hdr, sizeof(msg));

  return co_ssl_socket_send(ssl_sock, &msg, sizeof(msg));
}

bool corpc_proto_send_close_stream(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, uint16_t status)
{
  struct comsg_close_stream msg = {
    .hdr = {
      .code = htons(co_msg_close_stream_req),
      .size = htons(sizeof(msg.details)),
      .sid = htons(sid),
      .did = htons(did),
    },
    .details = {
      .status = htons(status)
    }
  };

  set_crc(&msg.hdr, sizeof(msg));

  return co_ssl_socket_send(ssl_sock, &msg, sizeof(msg));
}

bool corpc_proto_send_data(co_ssl_socket * ssl_sock, uint16_t sid, uint16_t did, const void * data, size_t size)
{
  struct comsghdr msghdr = {
    .code = htons(co_msg_data),
    .size = htons(size),
    .sid = sid,
    .did = did,
  };

  bool fok = false;

  msghdr.crc = crc_final(crc_update(crc_update(crc_begin(), (const uint8_t*) &msghdr + sizeof(msghdr.crc),
      sizeof(msghdr) - sizeof(msghdr.crc)), data, size));

  if ( (fok = co_ssl_socket_send(ssl_sock, &msghdr, sizeof(msghdr))) ) {
    fok = co_ssl_socket_send(ssl_sock, data, size);
  }

  return fok;
}
