/*
 * hexbits.c
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 */

#include "cuttle/hexbits.h"
#include <ctype.h>

static int ctoh(char c)
{
  if ( c >= '0' && c <= '9' ) {
    return c - '0';
  }
  if ( c >= 'A' && c <= 'F' ) {
    return c - 'A' + 10;
  }
  if ( c >= 'a' && c <= 'f' ) {
    return c - 'a' + 10;
  }
  return -1;
}

char * cf_bits2hex(const void * bits, size_t cbbits, char str[/*2*cbbits+1*/])
{
  static const char hex[] = "0123456789ABCDEF";
  const unsigned char * pb = bits;
  char * s;
  for ( s = str; cbbits--; ++pb ) {
    *s++ = hex[((*pb) >> 4) & 0x0F];
    *s++ = hex[(*pb) & 0x0F];
  }
  *s = 0;
  return str;
}

size_t cf_hex2bits(const char * hex, void * bits, size_t cbbitsmax)
{
  unsigned char * pbits = bits;
  int c1 = -1, c2 = -1;
  size_t i;

  while ( isspace(*hex) ) {
    ++hex;
  }

  for ( i = 0; *hex && !isspace(*hex) && i < cbbitsmax && (c1 = ctoh(*hex)) >= 0 && (c2 = ctoh(*(hex + 1))) >= 0; ++i, hex += 2 ) {
    pbits[i] = (unsigned char) ((c1 << 4) | (c2));
  }

  return c1 >= 0 && c2 >= 0 ? i : 0;
}
