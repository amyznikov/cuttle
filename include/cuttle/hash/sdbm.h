/*
 * sdbm.h
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 *
 *  http://www.cse.yorku.ca/~oz/hash.html
 *
 *  Result is in big-endian byte order (also referred to as network byte order)
 */

// #pragma once

#ifndef __cuttle_hash_sdbm_h__
#define __cuttle_hash_sdbm_h__

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


uint32_t cf_sdbm_begin(void);
uint32_t cf_sdbm_update(uint32_t h, const void * p, size_t n);
uint32_t cf_sdbm_update_s(uint32_t h, const char * s);
uint32_t cf_sdbm_finalize(uint32_t h);

static inline uint32_t cf_sdbm(const void * p, size_t n) {
  return cf_sdbm_finalize(cf_sdbm_update(cf_sdbm_begin(), p, n));
}

static inline uint32_t cf_sdbm_s(const char * s) {
  return cf_sdbm_finalize(cf_sdbm_update_s(cf_sdbm_begin(), s));
}


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_hash_sdbm_h__ */
