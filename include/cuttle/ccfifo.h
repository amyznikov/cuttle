/*
 * ccfifo.h
 *
 *  Created on: Sep 12, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_fifo_h__
#define __cuttle_fifo_h__


#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef
struct ccfifo {
  void * items;
  size_t capacity;
  size_t size;
  size_t item_size;
  size_t first;
  size_t last;
} ccfifo;


#define ccfifo_item_type(q)  uint8_t(*)[(q)->item_size]
#define ccfifo_item(q,pos)   (((ccfifo_item_type(q))(q)->items)+(pos))


static inline bool ccfifo_init(ccfifo * q, size_t capacity, size_t item_size)
{
  if ( (q->items = malloc(capacity * item_size)) ) {
    q->capacity = capacity;
    q->item_size = item_size;
    q->size = q->first = q->last = 0;
  }
  return q->items != NULL;
}

static inline void ccfifo_cleanup(ccfifo * q)
{
  free(q->items);
  memset(q, 0, sizeof(*q));
}


static inline void * ccfifo_push_bytes(ccfifo * q, const void * item, size_t size)
{
  void * itempos = NULL;

  if ( q->size < q->capacity ) {

    itempos = ccfifo_item(q, q->last);

    if ( item && size ) {
      memcpy(itempos, item, size );
    }

    ++q->size;
    if ( ++ q->last >= q->capacity ) {
      q->last = 0;
    }
  }

  return itempos;
}

static inline bool ccfifo_pop_bytes(ccfifo * q, void * pitem, size_t size)
{
  if ( !q->size ) {
    return false;
  }

  if ( pitem && size ) {
    memcpy(pitem, ccfifo_item(q, q->first), size);
  }

  --q->size;
  if ( ++q->first >= q->capacity ) {
    q->first = 0;
  }

  return true;
}

static inline void * ccfifo_push(ccfifo * q, const void * pitem)
{
  return ccfifo_push_bytes(q, pitem, q->item_size);
}

static inline void * ccfifo_ppush(ccfifo * q, const void * item)
{
  return ccfifo_push(q, &item);
}

static inline bool ccfifo_pop(ccfifo * q, void * pitem)
{
  return ccfifo_pop_bytes(q, pitem, q->item_size);
}

static inline void * ccfifo_ppop(ccfifo * q)
{
  void * item = NULL;
  ccfifo_pop(q, &item);
  return item;
}

static inline void * ccfifo_peek(const ccfifo * q, size_t index)
{
  return ccfifo_item(q, q->first + index);
}

static inline void * ccfifo_ppeek(const ccfifo * q, size_t index)
{
  return *(void**)(ccfifo_item(q, q->first + index));
}

static inline void * ccfifo_peek_front(const ccfifo * q)
{
  return (q->size ? ccfifo_item(q, q->first) : NULL);
}

static inline void * ccfifo_ppeek_front(const ccfifo * q)
{
  return (q->size ? *(void**)(ccfifo_item(q, q->first)) : NULL);
}

static inline size_t ccfifo_size(const ccfifo * q)
{
  return q->size;
}

static inline size_t ccfifo_capacity(const ccfifo * q)
{
  return q->capacity;
}

static inline bool ccfifo_is_full(const ccfifo * q)
{
  return q->size == q->capacity;
}

static inline bool ccfifo_is_empty(const ccfifo * q)
{
  return q->size == 0;
}



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_fifo_h__ */
