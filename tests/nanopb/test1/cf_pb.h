/*
 * cf_pb.h
 *
 *  Created on: Sep 26, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef TESTS_NANOPB_TEST1_CF_PB_H_
#define TESTS_NANOPB_TEST1_CF_PB_H_

#include <cuttle/debug.h>
#include <cuttle/nanopb/pb_encode.h>
#include <cuttle/nanopb/pb_decode.h>
#include <cuttle/ccarray.h>
#include <cuttle/membuf.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef
enum cf_pb_field_type {
  CF_PB_INT32 = 1,
  CF_PB_UINT32,
  CF_PB_INT64,
  CF_PB_UINT64,
  CF_PB_FIXED32,
  CF_PB_SFIXED32,
  CF_PB_FIXED64,
  CF_PB_SFIXED64,
  CF_PB_FLOAT,
  CF_PB_DOUBLE,
  CF_PB_BOOL,
  CF_PB_STRING,
  CF_PB_BYTES,
  CF_PB_MESSAGE,
} cf_pb_field_type;

typedef
enum cf_pb_field_alloc_type {
  CF_PB_SCALAR,
  CF_PB_ARRAY,
  CF_PB_ONEOF,
} cf_pb_field_alloc_type;

typedef
struct cf_pb_field_t {
  uint32_t  tag;
  uint8_t   pbtype;
  uint8_t   alloctype;
  uint16_t  item_size;
  uint16_t  item_offset;
  uint16_t  has_offset;
  const void * ptr;
} cf_pb_field_t;


#define CF_PB_REQUIRED_FIELD(st, tag, pbtype, alloc, memb, ctype, ptr) \
    {tag, pbtype, alloc, sizeof(ctype), offsetof(struct st, memb), (uint16_t)(-1), ptr}

#define CF_PB_OPTIONAL_FIELD(st, tag, pbtype, alloc, memb, ctype, ptr) \
    {tag, pbtype, alloc, sizeof(ctype), offsetof(struct st, memb), offsetof(struct st, has_##memb), ptr}

#define CF_PB_ONEOF_FIELD(st, ftag, pbtype, oneof, memb, ctype, ptr) \
    {ftag, pbtype, CF_PB_ONEOF, sizeof(ctype), offsetof(struct st, oneof.memb), offsetof(struct st, oneof.tag), ptr}

#define CF_PB_LAST_FIELD \
    {0}


bool cf_pb_get_encoded_size(size_t * size, const cf_pb_field_t fields[], const void * msg);
bool cf_pb_encode(pb_ostream_t * ostream, const cf_pb_field_t fields[], const void * msg);
bool cf_pb_decode(pb_istream_t * istream, const cf_pb_field_t fields[], void * msg);

#ifdef __cplusplus
}
#endif

#endif /* TESTS_NANOPB_TEST1_CF_PB_H_ */
