/*
 * djb2.h
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

// #pragma once

#ifndef __cuttle_hash_djb2_h__
#define __cuttle_hash_djb2_h__

#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


uint32_t cf_djb2_begin();
uint32_t cf_djb2_update(uint32_t h, const void * p, size_t n);
uint32_t cf_djb2_update_s(uint32_t h, const char * s);
uint32_t cf_djb2_finalize(uint32_t h);

static inline uint32_t cf_djb2(const void * p, size_t n) {
  return cf_djb2_finalize(cf_djb2_update(cf_djb2_begin(), p, n));
}
static inline uint32_t cf_djb2_s(const char * s) {
  return cf_djb2_finalize(cf_djb2_update_s(cf_djb2_begin(), s));
}



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_hash_djb2_h__ */
