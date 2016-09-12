/*
 * sdbm.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 *
 *
 *  http://www.cse.yorku.ca/~oz/hash.html
 *
 *  Result is in big-endian byte order (also referred to as network byte order)
 */

#include <cuttle/hash/sdbm.h>
#include <endian.h>

uint32_t cf_sdbm_begin(void)
{
  return 0;
}

uint32_t cf_sdbm_update(uint32_t h, const void * p, size_t n)
{
  const uint8_t * s = p;
  while ( n-- ) {
    h = *s++ + (h << 6) + (h << 16) - h;
  }
  return h;
}

uint32_t cf_sdbm_update_s(uint32_t h, const char * s)
{
  while ( *s ) {
    h = (uint8_t)*s++ + (h << 6) + (h << 16) - h;
  }
  return h;
}

uint32_t cf_sdbm_finalize(uint32_t h)
{
  return htobe32(h);
}

