/*
 * nanopb-test.c
 *
 *  Created on: Sep 25, 2016
 *      Author: amyznikov
 */

#include <cuttle/debug.h>
#include <cuttle/hexbits.h>
#include <cuttle/nanopb/pb_encode.h>
#include <cuttle/nanopb/pb_decode.h>
#include <malloc.h>


static size_t pb_get_enc_size(const void * message, const pb_field_t fields[])
{
  pb_ostream_t stream = PB_OSTREAM_SIZING;
  if ( !pb_encode(&stream, fields, message) ) {
    CF_CRITICAL("pb_encode(SIZING) fails: %s", PB_GET_ERROR(&stream));
    return 0;
  }
  return stream.bytes_written;
}

static size_t cf_pb_pack(const void * message, const pb_field_t fields[], void ** buf)
{
  size_t size;

  *buf = NULL;

  if ( !(size = pb_get_enc_size(message, fields)) ) {
    CF_FATAL("pb_get_enc_size() fails");
  }
  else if ( !(*buf = malloc(size)) ) {
    CF_FATAL("malloc(buf) fails: %s", strerror(errno));
  }
  else {
    pb_ostream_t stream = pb_ostream_from_buffer(*buf, size);
    if ( !pb_encode(&stream, fields, message) ) {
      CF_FATAL("pb_encode() fails");
      size = 0;
    }
  }

  if ( !size && *buf ) {
    free(*buf), *buf = NULL;
  }

  return size;
}

static bool cf_pb_unpack(const void * buf, size_t size, const pb_field_t fields[], void * message)
{
  pb_istream_t stream = pb_istream_from_buffer(buf, size);
  if ( !pb_decode(&stream, fields, message) ) {
    CF_FATAL("pb_decode() fails: %s", PB_GET_ERROR(&stream));
    return false;
  }
  return true;
}




typedef
struct message {
//  int32_t x;
//  int32_t y;
//  char * s;
  pb_bytes_array_t * bytes;
} message;


static const pb_field_t pb_message_fields[] = {
//  PB_FIELD(  1, INT32   , REQUIRED, STATIC  , FIRST, message, x, x, 0),
//  PB_FIELD(  2, INT32   , REQUIRED, STATIC  , OTHER, message, y, x, 0),
//  PB_FIELD(  3, STRING  , REQUIRED, POINTER , OTHER, message, s, y, NULL),
//  PB_FIELD(  4, BYTES   , REQUIRED, POINTER , OTHER, message, bytes, s, NULL),
  PB_FIELD(  4, BYTES   , REQUIRED, POINTER , FIRST, message, bytes, bytes, NULL),
  PB_LAST_FIELD
};

static size_t cf_pb_pack_message(const struct message * message, void ** buf)
{
  return cf_pb_pack(message, pb_message_fields, buf);
}

static bool cf_pb_unpack_message(const void * buf, size_t size, struct message * message)
{
  return cf_pb_unpack(buf, size, pb_message_fields, message);
}

static char * sbytes(const pb_bytes_array_t * arr)
{
  static char buf[1024];
  return arr ? cf_bits2hex(arr->bytes, arr->size, buf) : "(null)";
}

static void dump_msg(const char * note, const struct message * msg)
{
//  CF_DEBUG("%s: %p {.x=%d .y=%d .s=%s .bytes=%p\n", note, msg, msg->x, msg->y, msg->s, msg->bytes);
  CF_DEBUG("%s: %p {.bytes=%p {%s} }\n", note, msg, msg->bytes, sbytes(msg->bytes));
}

static void dump_fields(const char * msg,  const pb_field_t fields[])
{
  CF_NOTICE("<%s FIELDS>", msg);
  for ( const pb_field_t * f = fields; f->tag; ++f ) {
    CF_NOTICE("  tag=%u type=%u data_offset=%u size_offset=%u data_size=%u array_size=%u ptr=%p",
        f->tag, f->type, f->data_offset, f->size_offset, f->data_size, f->array_size, f->ptr);
  }
  CF_NOTICE("</%s FIELDS>", msg);
}


static char testbuf[1024] = "Long line from Hello World message";

int main(/*int argc, char *argv[]*/)
{
  struct message srcmsg = {
//    .x = 10,
//    .y = 125,
//    .s = testbuf
  };



  struct message dstmsg = {
//    .x = 0,
//    .y = 0,
//    .s = NULL
  };


  void * buf = NULL;
  size_t size;

  cf_set_loglevel(CF_LOG_DEBUG);
  cf_set_logfilename("stderr");


  srcmsg.bytes = malloc(sizeof(pb_bytes_array_t) + 6 * sizeof(srcmsg.bytes->bytes[0]));
  srcmsg.bytes->size = 6;
  for ( uint i = 0; i < srcmsg.bytes->size; ++i ) {
    srcmsg.bytes->bytes[i] = i + 1;
  }


  CF_DEBUG("testbuf=%p\n", testbuf);

  dump_fields("start", pb_message_fields);

  dump_msg("1 srcmsg", &srcmsg);
  dump_msg("1 dstmsg", &dstmsg);

  if ( !(size = cf_pb_pack_message(&srcmsg, &buf)) ) {
    CF_FATAL("cf_pb_pack_message() fails: %s", strerror(errno));
    goto end;
  }

  dump_msg("2 srcmsg", &srcmsg);
  dump_msg("2 dstmsg", &dstmsg);


  if ( !cf_pb_unpack_message(buf, size, &dstmsg) ) {
    CF_FATAL("cf_pb_unpack_message() fails: %s", strerror(errno));
    goto end;
  }

  dump_msg("3 srcmsg", &srcmsg);
  dump_msg("3 dstmsg", &dstmsg);

  dump_fields("end", pb_message_fields);

end:

  free(buf);
  //free(dstmsg.s);

  return 0;
}
