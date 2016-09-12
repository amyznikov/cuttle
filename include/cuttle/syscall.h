/*
 * syscall.h
 *
 *  Created on: Sep 9, 2016
 *      Author: amyznikov
 */

#pragma once

#ifndef ___cuttle_syscall_h___
#define ___cuttle_syscall_h___

#include <unistd.h>
#include <syscall.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline pid_t cf_gettid(void)
{
  return (pid_t) syscall (SYS_gettid);
}

#ifdef __cplusplus
}
#endif

#endif /* ___cuttle_syscall_h___ */
