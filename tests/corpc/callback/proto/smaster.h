/*
 * smaster.h
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __tests_corpc_callback_proto_smaster_h__
#define __tests_corpc_callback_proto_smaster_h__

#include <cuttle/corpc/channel.h>

#ifdef __cplusplus
extern "C" {
#endif

#define k_smaster_events_service_name "smaster.events"
#define k_smaster_events_ontimer_methd_name "ontimer"


typedef
struct sm_timer_event {
  char * msg;
} sm_timer_event;

void init_sm_timer_event(struct sm_timer_event * e, const char * msg);
void cleanup_sm_timer_event(struct sm_timer_event * e);
bool corpc_stream_write_sm_timer_event(corpc_stream * st, const struct sm_timer_event * e);
bool corpc_stream_read_sm_timer_event(corpc_stream * st, struct sm_timer_event * e);










#ifdef __cplusplus
}
#endif

#endif /* __tests_corpc_callback_proto_smaster_h__ */
