/*
 * hexbits.h
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_hexbits_h__
#define __cuttle_hexbits_h__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

char * cf_bits2hex(const void * bits, size_t cbbits, char str[/*2*cbbits+1*/]);
size_t cf_hex2bits(const char * hex, void * bits, size_t cbbitsmax);


#define cf_sbits2hex(bits, size) \
    cf_bits2hex((bits), (size), (char[2*(size)+1]){0})


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_hexbits_h__ */
