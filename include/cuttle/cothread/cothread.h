/*
 * cuttle/cothread/cothread.h
 *
 *  Created on: Sep 24, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_cothread_h__
#define __cuttle_cothread_h__

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef void * coroutine_t;

bool co_thread_init(void);
void co_thread_cleanup(void);

coroutine_t co_create(void (*func)(void *), void * data, void * stack, size_t size);
void co_delete(coroutine_t coro);
void co_call(coroutine_t coro);
void co_resume(void);
void co_exit_to(coroutine_t coro);
void co_exit(void);
coroutine_t co_current(void);
void * co_get_data(coroutine_t coro);
void * co_set_data(coroutine_t coro, void *data);
void * co_get_scheduler_data(coroutine_t coro);
void * co_set_scheduler_data(coroutine_t coro, void *data);
int co_get_min_stack_size(void);
void co_set_mem_allocator(void * (*alloc)(size_t), void (*free)(void *, size_t) );




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_cothread_h__ */
