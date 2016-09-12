/*
 * djb2.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 *
 *
 *  http://www.cse.yorku.ca/~oz/hash.html
 *
 *  Result is in big-endian byte order (also referred to as network byte order)
 */


#include <cuttle/hash/djb2.h>
#include <endian.h>


uint32_t cf_djb2_begin()
{
  return 5381;
}

uint32_t cf_djb2_update(uint32_t h, const void * p, size_t n)
{
  const uint8_t * s = p;
  while ( n-- ) {
    h = ((h << 5) + h) + (uint8_t) *s++;
  }
  return h;
}

uint32_t cf_djb2_update_s(uint32_t h, const char * s)
{
  while ( *s ) {
    h = ((h << 5) + h) + (uint8_t) *s++;
  }
  return h;
}

uint32_t cf_djb2_finalize(uint32_t h)
{
  return htobe32(h);
}
