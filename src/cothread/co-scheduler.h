/*
 * cuttle/src/cothread/co-scheduler.h
 *
 *  Created on: Sep 19, 2016
 *      Author: amyznikov
 */

#pragma once

#ifndef __cothread_scheduler_h__
#define __cothread_scheduler_h__

#include <cuttle/cothread/scheduler.h>
#include <cuttle/cothread/cothread.h>
#include <cuttle/pthread_wait.h>

#ifdef __cplusplus
extern "C" {
#endif

struct io_waiter {
  int64_t tmo;
  struct io_waiter *prev, *next;
  coroutine_t co;
  uint32_t mask;
  uint32_t events;
  uint32_t revents;
  uint32_t flags;
};

enum {
  iowait_io,
  iowait_eventfd
};

struct iorq {
  struct io_waiter * head, * tail;
  int so;
  int type;
};


struct co_socket {
  struct iorq e;
  int recvtmo, sendtmo;
};

bool co_socket_init(co_socket * cc, int so); // takes ownership
bool co_socket_create(co_socket * cc, int af, int sock_type, int proto);
bool co_socket_create_listening(co_socket * cc, const struct sockaddr * addrs, int sock_type, int proto);
bool co_socket_accept(co_socket * listenning, co_socket * accepted, struct sockaddr * addrs, socklen_t * addrslen);

#ifdef __cplusplus
}
#endif

#endif /* __cuttle_cothread_scheduler_h__ */
