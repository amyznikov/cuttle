/*
 * cf_pb_encode.c
 *
 *  Created on: Sep 26, 2016
 *      Author: amyznikov
 */

#include <cuttle/debug.h>
#include "cf_pb.h"


typedef bool (*cf_pb_encfn_t) (pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value);
typedef bool (*cf_pb_decfn_t) (pb_istream_t * istream, const cf_pb_field_t * field, void * dst);


static bool cf_pb_field_has_data(const cf_pb_field_t * field, const void * msg)
{
  bool fok;
  if ( field->alloctype == CF_PB_ONEOF ) {
    fok = *(uint32_t*) (msg + field->has_offset) == field->tag;
  }
  else {
    fok = (field->has_offset == (uint16_t) (-1)) || *(bool*) (msg + field->has_offset);
  }
  return fok;
}

static const cf_pb_field_t * cf_pb_find_tag(const cf_pb_field_t f[], uint32_t tag)
{
  while ( f->tag && f->tag != tag ) {
    ++f;
  }
  return f->tag ? f : NULL;
}

static pb_wire_type_t cf_pb_wire_type(cf_pb_field_type type)
{
  switch ( type ) {
  case CF_PB_INT32 :
    case CF_PB_UINT32 :
    case CF_PB_INT64 :
    case CF_PB_UINT64 :
    case CF_PB_BOOL :
    return PB_WT_VARINT;

  case CF_PB_FIXED32 :
    case CF_PB_SFIXED32 :
    case CF_PB_FLOAT :
    return PB_WT_32BIT;

  case CF_PB_FIXED64 :
    case CF_PB_SFIXED64 :
    case CF_PB_DOUBLE :
    return PB_WT_64BIT;

  case CF_PB_STRING :
    case CF_PB_BYTES :
    return PB_WT_STRING;

  case CF_PB_MESSAGE :
    return PB_WT_STRING;
  }

  return (pb_wire_type_t) (-1);
}

static bool cf_pb_type_packable(cf_pb_field_type type)
{
  switch ( type ) {
  case CF_PB_FIXED32 :
    case CF_PB_SFIXED32 :
    case CF_PB_FIXED64 :
    case CF_PB_SFIXED64 :
    case CF_PB_INT32 :
    case CF_PB_UINT32 :
    case CF_PB_INT64 :
    case CF_PB_UINT64 :
    return true;
  break;
  default :
    break;
  }
  return false;
}



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool cf_pb_encode_int32(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_svarint(ostream, *(int32_t*) value);
}

bool cf_pb_decode_int32(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  int64_t val;
  if ( pb_decode_svarint(istream, &val) && val >= INT32_MIN && val <= INT32_MAX ) {
    *(int32_t*) value = val;
    return true;
  }
  return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool cf_pb_encode_uint32(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_varint(ostream, *(uint32_t*) value);
}

bool cf_pb_decode_uint32(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  uint64_t val;
  if ( pb_decode_varint(istream, &val) && val <= UINT32_MAX ) {
    *(uint32_t*) value = val;
    return true;
  }
  return false;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool cf_pb_encode_int64(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_svarint(ostream, *(int64_t*) value);
}

bool cf_pb_decode_int64(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  return pb_decode_svarint(istream, (int64_t*) value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool cf_pb_encode_uint64(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_varint(ostream, *(uint64_t *) value);
}

bool cf_pb_decode_uint64(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  return pb_decode_varint(istream, (uint64_t*) value);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool cf_pb_encode_fixed32(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_fixed32(ostream, value);
}

bool cf_pb_decode_fixed32(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  return pb_decode_fixed32(istream, (uint32_t*)value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool cf_pb_encode_fixed64(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_fixed64(ostream, value);
}

bool cf_pb_decode_fixed64(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  return pb_decode_fixed64(istream, value);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



bool cf_pb_encode_bool(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_varint(ostream, *(bool *) value);
}

bool cf_pb_decode_bool(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  uint64_t boolval = 0;
  if ( pb_decode_varint(istream, &boolval) ) {
    *(bool*) value = boolval != 0;
    return true;
  }
  return false;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool cf_pb_encode_float(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_fixed32(ostream, value);
}

bool cf_pb_decode_float(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  return pb_decode_fixed32(istream, value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool cf_pb_encode_double(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  return pb_encode_fixed64(ostream, value);
}

bool cf_pb_decode_double(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  return pb_decode_fixed64(istream, value);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool cf_pb_encode_string(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  const char * str = *(const char **) value;
  return pb_encode_string(ostream, (const pb_byte_t *) str, strlen(str));
}


bool cf_pb_decode_string(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  uint64_t size;
  size_t alloc_size;
  char * str = NULL;
  bool fok = false;

  if ( !pb_decode_varint(istream, &size) ) {
    goto end;
  }

  /* Space for null terminator */
  if ( (alloc_size = size + 1) < size ) {
    PB_SET_ERROR(istream, "size too large");
    goto end;
  }

  if ( !(str = malloc(alloc_size)) ) {
    PB_SET_ERROR(istream, "malloc() fails");
    goto end;
  }

  if ( !(fok = pb_read(istream, (pb_byte_t*) str, size)) ) {
    PB_SET_ERROR(istream, "pb_read() fails");
    goto end;
  }

  *((pb_byte_t*) str + size) = 0;

end :

  if ( !fok ) {
    if ( str ) {
      free(str), str = NULL;
    }
  }

  *(char**)value = str;

  return fok;
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


bool cf_pb_encode_bytes(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  (void)(field);
  const cf_membuf * data = value;
  return pb_encode_string(ostream, data->data, data->size);
}

bool cf_pb_decode_bytes(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  (void)(field);
  uint64_t size;
  cf_membuf * mbuf = value;
  bool fok = false;

  if ( !pb_decode_varint(istream, &size) ) {
    goto end;
  }

  if ( size > 64 * 1024 ) {
    PB_SET_ERROR(istream, "Too large array");
    goto end;
  }

  if ( !(mbuf->data = malloc((size_t) (size))) ) {
    PB_SET_ERROR(istream, "malloc() fails");
    goto end;
  }

  if ( !(fok = pb_read(istream, (pb_byte_t*) mbuf->data, size)) ) {
    PB_SET_ERROR(istream, "pb_read() fails");
    goto end;
  }

  mbuf->size = (size_t)size;

end :

  if ( !fok && mbuf->data ) {
    free(mbuf->data), mbuf->data = NULL;
  }

  return fok;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool cf_pb_encode_submessage(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  const cf_pb_field_t * submessage_fields;
  pb_ostream_t substream = PB_OSTREAM_SIZING;
  size_t size;
  bool fok = false;

  if ( !(submessage_fields = field->ptr) ) {
    PB_SET_ERROR(ostream, "invalid field descriptor");
    goto end;
  }

  /* First calculate the message size using a non-writing substream. */
  if ( !cf_pb_encode(&substream, submessage_fields, value) ) {
    ostream->errmsg = substream.errmsg;
    goto end;
  }

  size = substream.bytes_written;
  if ( !pb_encode_varint(ostream, (uint64_t) size) ) {
    goto end;
  }

  if ( ostream->callback == NULL ) {
    fok = pb_write(ostream, NULL, size); /* Just sizing */
    goto end;
  }

  if ( ostream->bytes_written + size > ostream->max_size ) {
    PB_SET_ERROR(ostream, "stream full");
    goto end;
  }


  /* Use a substream to verify that a callback doesn't write more than
   * what it did the first time. */
  substream.callback = ostream->callback;
  substream.state = ostream->state;
  substream.max_size = size;
  substream.bytes_written = 0;
  substream.errmsg = NULL;


  fok = cf_pb_encode(&substream, submessage_fields, value);
  ostream->bytes_written += substream.bytes_written;
  ostream->state = substream.state;
  ostream->errmsg = substream.errmsg;

  if ( fok && substream.bytes_written != size ) {
    PB_SET_ERROR(ostream, "submsg size changed");
    fok = false;
  }

end:

  return fok;
}


bool cf_pb_decode_submessage(pb_istream_t * istream, const cf_pb_field_t * field, void * value)
{
  pb_istream_t substream;
  const cf_pb_field_t * submsg_fields;
  bool fok = false;

  if ( !(submsg_fields = field->ptr) ) {
    PB_SET_ERROR(istream, "invalid field descriptor");
    goto end;
  }

  if ( !pb_make_string_substream(istream, &substream) ) {
    goto end;
  }

  if ( !(fok = cf_pb_decode(&substream, submsg_fields, value)) ) {
    CF_FATAL("cf_pb_decode(substream) fails");
    PB_SET_ERROR(istream, substream.errmsg);
  }

  pb_close_string_substream(istream, &substream);

end :
  return fok;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

cf_pb_encfn_t cf_pb_encfunc(const cf_pb_field_t * field)
{
  cf_pb_encfn_t func = NULL;

  switch ( field->pbtype ) {
  case CF_PB_INT32 :
    func = cf_pb_encode_int32;
  break;
  case CF_PB_UINT32 :
    func = cf_pb_encode_uint32;
  break;
  case CF_PB_INT64 :
    func = cf_pb_encode_int64;
  break;
  case CF_PB_UINT64 :
    func = cf_pb_encode_uint64;
  break;
  case CF_PB_FIXED32 :
    case CF_PB_SFIXED32 :
    func = cf_pb_encode_fixed32;
  break;
  case CF_PB_FIXED64 :
    case CF_PB_SFIXED64 :
    func = cf_pb_encode_fixed64;
  break;
  case CF_PB_BOOL :
    func = cf_pb_encode_bool;
  break;
  case CF_PB_FLOAT:
    func = cf_pb_encode_float;
    break;
  case CF_PB_DOUBLE:
    func = cf_pb_encode_double;
    break;
  case CF_PB_STRING :
    func = cf_pb_encode_string;
  break;
  case CF_PB_BYTES :
    func = cf_pb_encode_bytes;
  break;
  case CF_PB_MESSAGE:
    func = cf_pb_encode_submessage;
    break;
  }

  return func;
}

cf_pb_decfn_t cf_pb_decfunc(const cf_pb_field_t * field)
{
  cf_pb_decfn_t func = NULL;

  switch ( field->pbtype ) {
  case CF_PB_INT32 :
    func = cf_pb_decode_int32;
  break;
  case CF_PB_UINT32 :
    func = cf_pb_decode_uint32;
  break;
  case CF_PB_INT64 :
    func = cf_pb_decode_int64;
  break;
  case CF_PB_UINT64 :
    func = cf_pb_decode_uint64;
  break;
  case CF_PB_FIXED32 :
    case CF_PB_SFIXED32 :
    func = cf_pb_decode_fixed32;
  break;
  case CF_PB_FIXED64 :
    case CF_PB_SFIXED64 :
    func = cf_pb_decode_fixed64;
  break;
  case CF_PB_BOOL :
    func = cf_pb_decode_bool;
  break;
  case CF_PB_FLOAT:
    func = cf_pb_decode_float;
    break;
  case CF_PB_DOUBLE:
    func = cf_pb_decode_double;
    break;
  case CF_PB_STRING :
    func = cf_pb_decode_string;
  break;
  case CF_PB_BYTES :
    func = cf_pb_decode_bytes;
  break;
  case CF_PB_MESSAGE:
    func = cf_pb_decode_submessage;
  break;
  }

  return func;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool cf_pb_encode_scalar(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value, cf_pb_encfn_t func)
{
  return pb_encode_tag(ostream, cf_pb_wire_type(field->pbtype), field->tag) && func(ostream, field, value);
}

bool cf_pb_encode_array(pb_ostream_t * ostream, const cf_pb_field_t * field, const ccarray_t * array, cf_pb_encfn_t func)
{
  pb_wire_type_t wiretype;
  size_t size, count, i;
  bool fok = false;

  if ( (count = ccarray_size(array)) < 1 ) {
    fok = true;
    goto end;
  }

  wiretype = cf_pb_wire_type(field->pbtype);

  /* Pack arrays if the datatype allows it. */
  if ( !cf_pb_type_packable(field->pbtype) ) {
    CF_DEBUG("UNPACKED ARRAY: count=%zu ostream->callback=%p", count, ostream->callback);

    for ( i = 0; i < count; ++i ) {
      if ( !pb_encode_tag(ostream, wiretype, field->tag) || !func(ostream, field, ccarray_peek(array, i)) ) {
        goto end;
      }
    }
  }
  else {

    CF_DEBUG("PACKED ARRAY: count=%zu ostream->callback=%p", count, ostream->callback);

    if ( !pb_encode_tag(ostream, PB_WT_STRING, field->tag) ) {
      goto end;
    }

    /* Determine the total size of packed array. */
    if ( wiretype == PB_WT_32BIT ) {
      size = 4 * count;
    }
    else if ( wiretype == PB_WT_64BIT ) {
      size = 8 * count;
    }
    else {
      pb_ostream_t sizestream = PB_OSTREAM_SIZING;
      for ( i = 0; i < count; ++i ) {
        if ( !func(&sizestream, field, ccarray_peek(array, i)) ) {
          goto end;
        }
      }
      size = sizestream.bytes_written;
      CF_DEBUG("pack size=%zu", size);
    }

    if ( !pb_encode_varint(ostream, size) ) {
      goto end;
    }

    if ( ostream->callback == NULL ) { /* Just sizing.. */
      fok = pb_write(ostream, NULL, size);
      goto end;
    }

    /* Write the data */
    for ( i = 0; i < count; ++i ) {
      if ( !func(ostream, field, ccarray_peek(array, i)) ) {
        goto end;
      }
    }
  }

  fok = true;

end:

  return fok;
}



bool cf_pb_decode_array(pb_istream_t * istream, const cf_pb_field_t * field, ccarray_t * array, pb_wire_type_t wiretype,
    cf_pb_decfn_t func)
{
  pb_istream_t substream;
  bool fok = false;

  if ( wiretype == PB_WT_STRING && cf_pb_type_packable(field->pbtype) ) {

    /* Packed array */
    if ( !pb_make_string_substream(istream, &substream) ) {
      goto end;
    }

    if ( !ccarray_init(array, 2 * substream.bytes_left / field->item_size + 1, field->item_size) ) {
      PB_SET_ERROR(istream, "ccarray_init() fails");
      goto end;
    }

    CF_DEBUG("unpack size=%zu", substream.bytes_left);

    while ( substream.bytes_left && func(&substream, field, ccarray_peek_end(array)) ) {
      if ( ++array->size >= array->capacity ) {
        if ( !ccarray_realloc(array, array->capacity + 2 * substream.bytes_left / field->item_size + 1) ) {
          PB_SET_ERROR(istream, "ccarray_realloc() fails");
          break;
        }
      }
    }

    pb_close_string_substream(istream, &substream);

    if ( substream.bytes_left != 0 ) {
      PB_SET_ERROR(istream, "substream read error");
      goto end;
    }
  }
  else {

    if ( !ccarray_capacity(array) && !ccarray_init(array, 64, field->item_size) ) {
      PB_SET_ERROR(istream, "ccarray_init() fails");
      goto end;
    }

    if ( ccarray_size(array) >= ccarray_capacity(array) && !ccarray_realloc(array, ccarray_capacity(array) + 64) ) {
      PB_SET_ERROR(istream, "ccarray_realloc() fails");
      goto end;
    }

    if ( !func(istream, field, ccarray_peek_end(array)) ) {
      goto end;
    }

    ++array->size;
  }

  fok = true;

end:
  return fok;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





bool cf_pb_encode_field(pb_ostream_t * ostream, const cf_pb_field_t * field, const void * value)
{
  cf_pb_encfn_t func = NULL;
  bool fok = false;

  if ( !(func = cf_pb_encfunc(field)) ) {
    PB_SET_ERROR(ostream, "Invalid field type");
    goto end;
  }

  switch ( field->alloctype ) {

  case CF_PB_SCALAR :
    fok = cf_pb_encode_scalar(ostream, field, value, func);
  break;

  case CF_PB_ARRAY :
    fok = cf_pb_encode_array(ostream, field, value, func);
  break;

  case CF_PB_ONEOF :
    fok = cf_pb_encode_scalar(ostream, field, value, func);
  break;

  default :
    PB_SET_ERROR(ostream, "Invalid field alloc type");
  break;
  }

end:
  return fok;
}

bool cf_pb_decode_field(pb_istream_t * istream, pb_wire_type_t wiretype, const cf_pb_field_t * field, void * value)
{
  cf_pb_decfn_t func = NULL;
  bool fok = false;

  if ( !(func = cf_pb_decfunc(field)) ) {
    PB_SET_ERROR(istream, "Invalid field type");
    goto end;
  }

  switch ( field->alloctype ) {

  case CF_PB_ARRAY :
    fok = cf_pb_decode_array(istream, field, value, wiretype, func);
  break;

  case CF_PB_SCALAR :
    if ( wiretype != cf_pb_wire_type(field->pbtype) ) {
      PB_SET_ERROR(istream, "Invalid wire type");
    }
    else {
      fok = func(istream, field, value);
    }
  break;

  case CF_PB_ONEOF :
    if ( wiretype != cf_pb_wire_type(field->pbtype) ) {
      PB_SET_ERROR(istream, "Invalid wire type");
    }
    else {
      fok = func(istream, field, value);
    }
    break;


  default :
    PB_SET_ERROR(istream, "Invalid field alloc type");
  break;
  }

end:
  return fok;
}




///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool cf_pb_get_encoded_size(size_t * size, const cf_pb_field_t fields[], const void * msg)
{
  pb_ostream_t stream = PB_OSTREAM_SIZING;
  bool fok = cf_pb_encode(&stream, fields, msg);
  if ( fok ) {
    *size = stream.bytes_written;
  }
  return fok;
}


bool cf_pb_encode(pb_ostream_t * ostream, const cf_pb_field_t fields[], const void * msg)
{
  bool fok = true;

  for ( const cf_pb_field_t * field = fields; field->tag; ++field ) {
    if ( cf_pb_field_has_data(field, msg) && !cf_pb_encode_field(ostream, field, msg + field->item_offset) ) {
      CF_FATAL("cf_pb_encode_field() fails");
      fok = false;
      break;
    }
  }

  return fok;
}


bool cf_pb_decode(pb_istream_t * istream, const cf_pb_field_t fields[], void * msg)
{
  const cf_pb_field_t * field;
  pb_wire_type_t wiretype;
  uint32_t tag;
  bool eof = false, fok = true;

  while ( istream->bytes_left && pb_decode_tag(istream, &wiretype, &tag, &eof) ) {

    if ( !(field = cf_pb_find_tag(fields, tag)) ) { /* No match found, skip data */
      if ( !(fok = pb_skip_field(istream, wiretype)) ) {
        break;
      }
      CF_WARNING("tag=%u not found", tag);
      continue;
    }

    if ( !(fok = cf_pb_decode_field(istream, wiretype, field, msg + field->item_offset)) ) {
      break;
    }

    if ( field->alloctype == CF_PB_ONEOF ) {
      *(uint32_t*) (msg + field->has_offset) = field->tag;
    }
    else if ( field->has_offset != (uint16_t) (-1) ) {
      *(bool *) (msg + field->has_offset) = true;
    }
  }

  if ( istream->bytes_left ) {
    fok = false;
  }

  return fok;
}
