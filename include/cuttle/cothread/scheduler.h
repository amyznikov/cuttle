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
bool co_schedule_io(int so, uint32_t events, int (*callback)(void * arg, uint32_t events), void * arg, size_t stack_size);
bool cf_in_co_thread(void);
void co_yield(void);
void co_sleep(uint32_t msec);
uint32_t co_io_wait(int so, uint32_t events, int msec);

////////////////////////////////////////////////////////////////


typedef
struct co_thread_lock_s
  * co_thread_lock_t;

#define CO_THREAD_LOCK_INITIALIZER  NULL

bool co_thread_lock_init(co_thread_lock_t * wait);
void co_thread_lock_destroy(co_thread_lock_t * wait);
bool co_thread_lock(co_thread_lock_t * wait);
bool co_thread_unlock(co_thread_lock_t * wait);
int  co_thread_signal(co_thread_lock_t * wait);
int  co_thread_broadcast(co_thread_lock_t * wait);
int  co_thread_wait(co_thread_lock_t * wait, int tmout);

////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////

typedef
struct co_socket
  co_socket;

co_socket * co_socket_init_new(int so); // takes ownership
co_socket * co_socket_create_new(int af, int sock_type, int proto);
co_socket * co_socket_create_listening_new(const struct sockaddr * addrs, int sock_type, int proto);
co_socket * co_socket_accept_new(co_socket * listenning, struct sockaddr * addrs, socklen_t * addrslen);
co_socket * co_socket_connect_new(const struct sockaddr *address, int sock_type, int proto, int tmo_ms);

void co_socket_close(co_socket * cc, bool abort_conn);
void co_socket_destroy(co_socket ** cc, bool abort_conn);

bool co_socket_get_peername(const co_socket * cc, struct sockaddr * addrs, socklen_t * addrslen);
bool co_socket_get_sockname(const co_socket * cc, struct sockaddr * addrs, socklen_t * addrslen);


int  co_socket_fd(const co_socket * cc);
bool co_socket_set_send_tmout(co_socket * cc, int msec);
bool co_socket_set_recv_tmout(co_socket * cc, int msec);
bool co_socket_set_sndrcv_tmouts(co_socket * cc, int snd_tmout_msec, int rcv_tmout_msec);
bool co_socket_connect(co_socket * cc, const struct sockaddr *address, int tmo_ms);
ssize_t co_socket_send(co_socket * cc, const void * buf, size_t size, int flags);
ssize_t co_socket_recv(co_socket * cc, void * buf, size_t size, int flags);



////////////////////////////////////////////////////////////////

// Sockets MUST be in non-blocking mode
ssize_t co_send(int so, const void * buf, size_t size, int flags);
ssize_t co_recv(int so, void * buf, size_t size, int flags);
ssize_t co_read(int fd, void * buf, size_t size);
ssize_t co_write(int fd, const void *buf, size_t size);
int co_connect(int so, const struct sockaddr *address, socklen_t address_len);
int co_tcp_connect(const struct sockaddr *address, socklen_t address_len, int tmo_sec);
int co_accept(int so, struct sockaddr * restrict address, socklen_t * restrict address_len);
int co_poll(struct pollfd * fds, nfds_t nfds, int timeout_ms);


////////////////////////////////////////////////////////////////


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_cothread_scheduler_h__ */
