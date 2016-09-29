/*
 * nanopb-test.c
 *
 *  Created on: Sep 25, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include <math.h>
#include <stdio.h>
#include <cuttle/debug.h>
#include <cuttle/hexbits.h>
#include <cuttle/pb/pb.h>




typedef
struct submessage {
  char * s;
} submessage;


enum oneofmessage_case {
  oneofmessage_tag_nothing = 0,
  oneofmessage_tag_s = 12,
  oneofmessage_tag_x = 13,
};

typedef
struct oneofmessage {
  uint32_t tag;
  union {
    char * s;
    double x;
  };
} oneofmessage;


typedef
struct message {
  int64_t x;
  uint32_t y;
  float f1;
  double d1;
  char * s;
  cf_membuf b;

  ccarray_t z;
  ccarray_t w;

  submessage sm;
  oneofmessage omsg;

  ccarray_t sma;

  ccarray_t dbla;

  bool has_b;
  bool has_z;
  bool has_w;
} message;



static const cf_pb_field_t pb_submessage_fields[] = {
  CF_PB_REQUIRED_FIELD(submessage, 1, CF_PB_STRING,    CF_PB_SCALAR, s, char*, NULL),
  CF_PB_LAST_FIELD
};


static const cf_pb_field_t pb_message_fields[] = {
  CF_PB_REQUIRED_FIELD(message,  1, CF_PB_INT64,    CF_PB_SCALAR, x, int64_t, NULL),
  CF_PB_REQUIRED_FIELD(message,  2, CF_PB_FIXED32,  CF_PB_SCALAR, y, uint32_t, NULL),
  CF_PB_REQUIRED_FIELD(message,  3, CF_PB_STRING,   CF_PB_SCALAR, s, char*, NULL),
  CF_PB_OPTIONAL_FIELD(message,  4, CF_PB_BYTES,    CF_PB_SCALAR, b, uint8_t, NULL),
  CF_PB_OPTIONAL_FIELD(message,  5, CF_PB_INT32,    CF_PB_ARRAY,  z, int32_t, NULL),
  CF_PB_OPTIONAL_FIELD(message,  6, CF_PB_STRING,   CF_PB_ARRAY,  w, char*, NULL),
  CF_PB_REQUIRED_FIELD(message,  7, CF_PB_MESSAGE,  CF_PB_SCALAR, sm, struct submessage, pb_submessage_fields),
  CF_PB_REQUIRED_FIELD(message,  8, CF_PB_MESSAGE,  CF_PB_ARRAY,  sma, struct submessage, pb_submessage_fields),
  CF_PB_REQUIRED_FIELD(message,  9, CF_PB_FLOAT,    CF_PB_SCALAR, f1, float, NULL),
  CF_PB_REQUIRED_FIELD(message, 10, CF_PB_DOUBLE,   CF_PB_SCALAR, d1, double, NULL),
  CF_PB_REQUIRED_FIELD(message, 11, CF_PB_DOUBLE,   CF_PB_ARRAY,  dbla, double, NULL),
  CF_PB_ONEOF_FIELD   (message, 12, CF_PB_STRING,   omsg,   s, char *, NULL),
  CF_PB_ONEOF_FIELD   (message, 13, CF_PB_DOUBLE,   omsg,   x, double, NULL),
  CF_PB_LAST_FIELD
};

size_t cf_pb_pack_message(const struct message * message, void ** buf)
{
  return cf_pb_pack(message, pb_message_fields, buf);
}

static bool cf_pb_unpack_message(const void * buf, size_t size, struct message * message)
{
  return cf_pb_unpack(buf, size, pb_message_fields, message);
}

static void dump_msg(const char * note, const struct message * msg)
{
  char hexbuf[msg->b.size * 2 + 1];

  CF_DEBUG("<%s>: x=%lld y=%d f1=%g d1=%g s='%s' .bytes={%zu: %s} .z={%zu} .sm='%s'",
      note, (long long)msg->x, msg->y, msg->f1, msg->d1, msg->s,
      msg->b.size, cf_bits2hex(msg->b.data, msg->b.size, hexbuf),
      ccarray_size(&msg->z),
      msg->sm.s);

  for ( size_t i = 0, n = ccarray_size(&msg->z); i < n; ++i ) {
    int32_t * v = ccarray_peek(&msg->z, i);
    CF_DEBUG("%s: z[%zu]=%d",note, i, *v);
  }

  for ( size_t i = 0, n = ccarray_size(&msg->w); i < n; ++i ) {
    char * str = ccarray_ppeek(&msg->w, i);
    CF_DEBUG("%s: z[%zu]='%s'",note, i, str);
  }

  for ( size_t i = 0, n = ccarray_size(&msg->sma); i < n; ++i ) {
    const struct submessage * sm = ccarray_peek(&msg->sma, i);
    CF_DEBUG("%s: sma[%zu]='%s'",note, i, sm->s);
  }

  for ( size_t i = 0, n = ccarray_size(&msg->dbla); i < n; ++i ) {
    const double * v = ccarray_peek(&msg->dbla, i);
    CF_DEBUG("%s: dbla[%zu]='%g'",note, i, *v);
  }


  CF_DEBUG("%s: omsg.tag='%u'",note, msg->omsg.tag);

  switch (msg->omsg.tag) {
    case oneofmessage_tag_nothing:
      CF_DEBUG("%s: omsg=NOTHING",note);
      break;
    case oneofmessage_tag_s:
      CF_DEBUG("%s: omsg.s='%s'",note, msg->omsg.s);
      break;
    case oneofmessage_tag_x:
      CF_DEBUG("%s: omsg.x='%f'",note, msg->omsg.x);
    break;
    default:
    break;
  }

  CF_DEBUG("</%s>\n\n",note);
}



int main(/*int argc, char *argv[]*/)
{
  struct message srcmsg = {
    .x = 10,
    .y = 125,
    .s = strdup("test message in srcmsg"),

    .has_b = true,
    .b = CF_MEMBUF(NULL, 0),

    .has_z = true,
    .z = CCARRAY_INITIALIZER,

    .has_w = true,
    .w = CCARRAY_INITIALIZER,

    .sm = {
      .s = strdup("submessage text")
    },

    .omsg = {
      .tag = oneofmessage_tag_x,
      .x = 127
    },
  };

  struct message dstmsg = {
    .x = 0,
    .y = 0,
    .s = NULL,
    .b = CF_MEMBUF(NULL,0),
  };

  void * buf = NULL;
  size_t size;

  cf_set_loglevel(CF_LOG_DEBUG);
  cf_set_logfilename("stderr");


  srcmsg.f1 = M_PI;
  srcmsg.d1 = M_E;

  size = 10;
  cf_membuf_set(&srcmsg.b, malloc(size), size);
  for ( uint i = 0; i < srcmsg.b.size; ++i ) {
    ((uint8_t*) srcmsg.b.data)[i] = i + 1;
  }

  size = 10;
  ccarray_init(&srcmsg.z, size, sizeof(int32_t));
  for ( uint i = 0, n = ccarray_capacity(&srcmsg.z); i < n; ++i ) {
    ccarray_push_back(&srcmsg.z, &i);
  }

  size = 4;
  ccarray_init(&srcmsg.w, size, sizeof(char*));
  for ( uint i = 0, n = ccarray_capacity(&srcmsg.w); i < n; ++i ) {
    char * str = NULL;
    asprintf(&str, "text message %u", i);
    ccarray_ppush_back(&srcmsg.w, str);
  }

  size = 4;
  ccarray_init(&srcmsg.sma, size, sizeof(struct submessage));
  for ( uint i = 0, n = ccarray_capacity(&srcmsg.sma); i < n; ++i ) {
    struct submessage sm;
    asprintf(&sm.s, "submessage text %u", i);
    ccarray_push_back(&srcmsg.sma, &sm);
  }


  size = 10;
  ccarray_init(&srcmsg.dbla, size, sizeof(double));
  for ( uint i = 0, n = ccarray_capacity(&srcmsg.dbla); i < n; ++i ) {
    double x = M_PI / i;
    ccarray_push_back(&srcmsg.dbla, &x);
  }

  dump_msg("srcmsg", &srcmsg);

  CF_DEBUG("XXXXX value=%p tag=%p %u", &srcmsg.omsg.x, &srcmsg.omsg.tag, srcmsg.omsg.tag);

  if ( !(size = cf_pb_pack_message(&srcmsg, &buf)) ) {
    CF_FATAL("cf_pb_pack_message() fails: %s", strerror(errno));
    goto end;
  }

  CF_DEBUG("pack size = %zu", size);


  if ( !cf_pb_unpack_message(buf, size, &dstmsg) ) {
    CF_FATAL("cf_pb_unpack_message() fails: %s", strerror(errno));
    goto end;
  }

  dump_msg("dstmsg", &dstmsg);

end:

  free(buf);

  return 0;
}


/*
 * size_t cf_pb_pack_all_types(const struct all_types * all_types, void ** buf)
{
  return cf_pb_pack(all_types, all_types_fields, buf);
}
 *
 */
