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



#define k_smaster_service_name "smaster"
#define k_smaster_sendfile_method_name "sendfile"

typedef
struct sm_sendfile_request {
  char * filename;
} sm_sendfile_request;

typedef
struct sm_sendfile_responce {
  char * resp;
} sm_sendfile_responce;

typedef
struct sm_sendfile_chunk {
  char * data;
} sm_sendfile_chunk;


void init_sm_sendfile_request(struct sm_sendfile_request * msg, const char * fname);
void cleanup_sm_sendfile_request(struct sm_sendfile_request * msg);
bool corpc_stream_write_sm_sendfile_request(corpc_stream * st, const struct sm_sendfile_request * sm_sendfile_request);
bool corpc_stream_read_sm_sendfile_request(corpc_stream * st, struct sm_sendfile_request * sm_sendfile_request);

void init_sm_sendfile_responce(struct sm_sendfile_responce * msg, const char * resp);
void cleanup_sm_sendfile_responce(struct sm_sendfile_responce * msg);
bool corpc_stream_write_sm_sendfile_responce(corpc_stream * st, const struct sm_sendfile_responce * sm_sendfile_responce);
bool corpc_stream_read_sm_sendfile_responce(corpc_stream * st, struct sm_sendfile_responce * sm_sendfile_responce);


void init_sm_sendfile_chunk(struct sm_sendfile_chunk * msg, const char * data);
void cleanup_sm_sendfile_chunk(struct sm_sendfile_chunk * msg);
bool corpc_stream_write_sm_sendfile_chunk(corpc_stream * st, const struct sm_sendfile_chunk * sm_sendfile_chunk);
bool corpc_stream_read_sm_sendfile_chunk(corpc_stream * st, struct sm_sendfile_chunk * sm_sendfile_chunk);



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
