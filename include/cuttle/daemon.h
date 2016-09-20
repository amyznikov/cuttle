/*
 * cuttle/daemon.h
 *
 *  Created on: Oct 2, 2015
 *      Author: amyznikov
 */


#ifndef __cuttle_daemon_h__
#define __cuttle_daemon_h__

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif




/**
 * fork() and become daemon
 *    see errno on failure
 */
pid_t cf_become_daemon(void);


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_daemon_h__ */
