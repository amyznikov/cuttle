/*
 * smaster.h
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __tests_corpc_callback_proto_smaster_h__
#define __tests_corpc_callback_proto_smaster_h__

#include "smaster.corpc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define k_smaster_events_service_name "smaster.events"
#define k_smaster_events_ontimer_method_name "ontimer"
#define k_smaster_ping_pong_method_name   "ping-pong"


void init_timer_event(struct timer_event * e, const char * msg);
void cleanup_timer_event(struct timer_event * e);










#ifdef __cplusplus
}
#endif

#endif /* __tests_corpc_callback_proto_smaster_h__ */
