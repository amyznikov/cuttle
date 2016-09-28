/*
 * membuf.h
 *
 *  Created on: Sep 26, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_membuf_h__
#define __cuttle_membuf_h__

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef
struct cf_membuf {
  void * data;
  size_t size;
} cf_membuf;

#define CF_MEMBUF_INITIALIZER {.data=NULL,.size=0}
#define CF_MEMBUF(_data,_size)  {.data=(_data),.size=(_size)}

static inline
void cf_membuf_init(cf_membuf * mb, void * data, size_t size) {
  mb->size = size, mb->data = data;
}

static inline
void cf_membuf_set(cf_membuf * mb, void * data, size_t size) {
  free(mb->data), mb->data = data, mb->size = size;
}

static inline
void cf_membuf_cleanup(cf_membuf * mb) {
  cf_membuf_set(mb, NULL, 0);
}


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_membuf_h__ */
