/*
 * corpc-msg.h
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_corpc_msg_h__
#define __cuttle_corpc_msg_h__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef
struct corpc_msg {
  ssize_t size;
  void * data;
} corpc_msg;

void corpc_msg_init(struct corpc_msg * comsg);
void corpc_msg_clean(struct corpc_msg * comsg);
void corpc_msg_set(struct corpc_msg * comsg, void * data, size_t size);


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_msg_h__ */
