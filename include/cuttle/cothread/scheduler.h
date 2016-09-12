/*
 * cuttle/cothread/scheduler.h
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_cothread_scheduler_h__
#define __cuttle_cothread_scheduler_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/poll.h>

#ifdef __cplusplus
extern "C" {
#endif

bool co_scheduler_init(int ncpu);
bool co_schedule(void (*fn)(void*), void * arg, size_t stack_size);
bool cf_in_cothread(void);
void co_yield(void);
void co_sleep(uint32_t msec);
uint32_t co_io_wait(int so, uint32_t events, int msec);

////////////////////////////////////////////////////////////////

typedef
struct co_mutex {
  void * data;
} co_mutex;

#define CO_MUTEX_INITIALIZER {NULL}

bool co_mutex_init(co_mutex * mtx);
bool co_mutex_lock(co_mutex * mtx);
bool co_mutex_unlock(co_mutex * mtx);
void co_mutex_destroy(co_mutex * mtx);


////////////////////////////////////////////////////////////////


typedef struct co_event
  co_event;

co_event * co_event_create(void);
void co_event_delete(co_event ** e);
bool co_event_set(co_event * e);


////////////////////////////////////////////////////////////////


typedef struct co_event_waiter
  co_event_waiter;

co_event_waiter * co_event_add_waiter(co_event * e);
void co_event_remove_waiter(co_event * e, co_event_waiter * w);
bool co_event_wait(co_event_waiter * w, int msec);



////////////////////////////////////////////////////////////////

typedef struct co_socket
  co_socket;

// takes ownership
co_socket * co_socket_new(int so);
co_socket * co_socket_connect_new(int so, const struct sockaddr *address, socklen_t addrslen, int tmo_ms);
co_socket * co_socket_accept_new(co_socket * cc, struct sockaddr * addrs, socklen_t * addrslen);

bool co_socket_connect(co_socket * cc, const struct sockaddr *address, socklen_t addrslen, int tmo_ms);
void co_socket_close(co_socket ** cc, bool abort_conn);
int  co_socket_fd(const co_socket * cc);
void co_socket_set_send_tmout(co_socket * cc, int msec);
void co_socket_set_recv_tmout(co_socket * cc, int msec);
void co_socket_set_sndrcv_tmouts(co_socket * cc, int snd_tmout_msec, int rcv_tmout_msec);
ssize_t co_socket_send(co_socket * cc, const void * buf, size_t size, int flags);
ssize_t co_socket_recv(co_socket * cc, void * buf, size_t size, int flags);



////////////////////////////////////////////////////////////////

// Sockets MUST be in non-blocking mode
ssize_t co_send(int so, const void * buf, size_t size, int flags);
ssize_t co_recv(int so, void * buf, size_t size, int flags);
ssize_t co_read(int fd, void * buf, size_t size);
ssize_t co_write(int fd, const void *buf, size_t size);
int co_connect(int so, const struct sockaddr *address, socklen_t address_len);
int co_accept(int so, struct sockaddr * restrict address, socklen_t * restrict address_len);
int co_poll(struct pollfd * fds, nfds_t nfds, int timeout_ms);


////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_cothread_scheduler_h__ */
