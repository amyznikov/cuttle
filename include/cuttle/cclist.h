/*
 * cclist.h
 *
 *  Created on: Mar 20, 2016
 *      Author: amyznikov
 */
//#pragma once

#ifndef __cqueue_h__
#define __cqueue_h__

#include <cuttle/ccheap.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef
struct cclist_node {
  struct cclist_node * prev, * next;
  union { // avoid dereferencing type-punned pointer, prevent breaking strict-aliasing rules
    void * p;
    uint8_t u;
  } item[];
} cclist_node;

typedef
struct cclist {
  ccheap heap;
  struct cclist_node * head, * tail;
  size_t capacity;
  size_t item_size;
} cclist;


static inline size_t cclist_node_size(const cclist * cc)
{
  return (sizeof(struct cclist_node) + cc->item_size);
}

static inline bool cclist_init(cclist * cc, size_t capacity, size_t item_size)
{
  cc->head = cc->tail = NULL;
  cc->capacity = capacity;
  cc->item_size = item_size;
  return ccheap_init(&cc->heap, capacity, cclist_node_size(cc));
}

static inline void cclist_cleanup(cclist * cc)
{
  if ( cc ) {
    ccheap_cleanup(&cc->heap);
    cc->head = cc->tail = NULL;
    cc->capacity = cc->item_size = 0;
  }
}


static inline struct cclist_node * cclist_head(const cclist * cc)
{
  return cc->head;
}

static inline struct cclist_node * cclist_tail(const cclist * cc)
{
  return cc->tail;
}

static inline void * cclist_peek(struct cclist_node * node)
{
  return node ? node->item : NULL;
}

static inline void * cclist_ppeek(const struct cclist_node * node)
{
  return node ? node->item[0].p : NULL;
}

static inline struct cclist_node * cclist_push(cclist * cc, struct cclist_node * after, const void * pitem)
{
  struct cclist_node * node = NULL;

  if ( !after && cc->head ) {
    errno = EINVAL;
  }
  else if ( (node = ccheap_alloc(&cc->heap)) ) {

    node->prev = after;

    if ( !cc->head ) {
      cc->head = cc->tail = node;
    }

    if ( !after ) {
      node->next = NULL;
    }
    else {

      node->next = after->next;
      after->next = node;

      if ( after == cc->tail ) {
        cc->tail = node;
      }
    }

    if ( pitem ) {
      memcpy(node->item, pitem, cc->item_size);
    }
  }

  return node;
}

static inline struct cclist_node * cclist_ppush(cclist * cc, struct cclist_node * after, const void * item)
{
  return cclist_push(cc, after, &item);
}

static inline struct cclist_node * cclist_push_back(cclist * cc, const void * pitem)
{
  return cclist_push(cc, cclist_tail(cc), pitem);
}

static inline struct cclist_node * cclist_ppush_back(cclist * cc, const void * item)
{
  return cclist_push_back(cc, &item);
}

static inline struct cclist_node * cclist_insert(cclist * cc, struct cclist_node * before, const void * pitem)
{
  struct cclist_node * node = NULL;

  if ( !before && cc->tail ) {
    errno = EINVAL;
  }
  else if ( (node = ccheap_alloc(&cc->heap)) ) {

    node->next = before;

    if ( !cc->tail ) {
      cc->tail = cc->head = node;
    }

    if ( !before ) {
      node->prev = NULL;
    }
    else {

      node->prev = before->prev;
      before->prev = node;

      if ( before == cc->head ) {
        cc->head = node;
      }
    }

    memcpy(node->item, pitem, cc->item_size);
  }

  return node;
}


static inline struct cclist_node * cclist_pinsert(cclist * cc, struct cclist_node * before, const void * item)
{
  return cclist_insert(cc, before, &item);
}


static inline void cclist_erase(cclist * cc, struct cclist_node * node)
{
  if ( node ) {

    struct cclist_node * prev = node->prev;
    struct cclist_node * next = node->next;

    if ( prev ) {
      prev->next = next;
    }

    if ( next ) {
      next->prev = prev;
    }

    if ( node == cc->head ) {
      cc->head = next;
    }

    if ( node == cc->tail ) {
      cc->tail = prev;
    }

    ccheap_free(&cc->heap, node);
  }
}


#ifdef __cplusplus
}
#endif

#endif /* __cqueue_h__ */
