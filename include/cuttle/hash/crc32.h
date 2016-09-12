/*
 * crc32.h
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 *
 *  Result is in big-endian byte order (also referred to as network byte order)
 */

#pragma once

#ifndef __cuttle_hash_crc32_h__
#define __cuttle_hash_crc32_h__

#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

uint32_t cf_crc32_begin(void);
uint32_t cf_crc32_update(uint32_t crc, const void * buff, size_t length);
uint32_t cf_crc32_update_s(uint32_t crc, const char * buff);
uint32_t cf_crc32_finalize(uint32_t crc);

static inline uint32_t cf_crc32(const void * buf, size_t length) {
  return cf_crc32_finalize(cf_crc32_update(cf_crc32_begin(), buf, length));
}

static inline uint32_t cf_crc32_s(const char * s) {
  return cf_crc32_finalize(cf_crc32_update_s(cf_crc32_begin(), s));
}


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_hash_crc32_h__ */
