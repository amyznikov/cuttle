/*
 * smaster.pb.h
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __tests_corpc_callback_proto_smaster_pb_h__
#define __tests_corpc_callback_proto_smaster_pb_h__

#include "smaster.h"

#ifdef __cplusplus
extern "C" {
#endif


ssize_t corpc_pack_sm_timer_event(const struct sm_timer_event * sm_timer_event, void ** data);
bool corpc_unpack_sm_timer_event(struct sm_timer_event * sm_timer_event, const void * data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __tests_corpc_callback_proto_smaster_pb_h__ */
