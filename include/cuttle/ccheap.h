/*
 * cheap.h
 *
 *  Created on: Sep 12, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_ccheap_h__
#define __cuttle_ccheap_h__

#include <cuttle/ccfifo.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef
struct ccheap {
  void * blocks;
  ccfifo fifo;
} ccheap;


static inline bool ccheap_init(ccheap * h, size_t max_blocks, size_t block_size)
{
  bool fok = false;

  memset(h, 0, sizeof(*h));

  if ( !(h->blocks = malloc(max_blocks * block_size)) ) {
    goto end;
  }

  if ( !ccfifo_init(&h->fifo, max_blocks, sizeof(void*)) ) {
    goto end;
  }

  for ( size_t i = 0; i < max_blocks; ++i ) {
    ccfifo_ppush(&h->fifo, (((uint8_t(*)[block_size])h->blocks) + max_blocks - i - 1));
  }

  fok = true;

end:
  if ( !fok ) {
    ccfifo_cleanup(&h->fifo);
    free(h->blocks), h->blocks = NULL;
  }

  return fok;
}

static inline void ccheap_cleanup(ccheap * h)
{
  if ( h ) {
    ccfifo_cleanup(&h->fifo);
    free(h->blocks), h->blocks = NULL;
  }
}

static inline void * ccheap_alloc(ccheap * h)
{
  void * pb = ccfifo_ppop(&h->fifo);
  if ( !pb ) {
    errno = ENOMEM;
  }
  return pb;
}

static inline void ccheap_free(ccheap * h, void * pb)
{
  ccfifo_ppush(&h->fifo, pb);
}

static inline bool ccheap_is_empty(const ccheap * h)
{
  return ccfifo_is_empty(&h->fifo);
}



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_ccheap_h__ */
