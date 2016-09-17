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


ssize_t corpc_pack_sm_sendfile_request(const struct sm_sendfile_request * sm_sendfile_request, void ** data);
bool corpc_unpack_sm_sendfile_request(struct sm_sendfile_request * sm_sendfile_request, const void * data, size_t size);

ssize_t corpc_pack_sm_sendfile_responce(const struct sm_sendfile_responce * sm_sendfile_responce, void ** data);
bool corpc_unpack_sm_sendfile_responce(struct sm_sendfile_responce * sm_sendfile_responce, const void * data, size_t size);

ssize_t corpc_pack_sm_sendfile_chunk(const struct sm_sendfile_chunk * sm_sendfile_chunk, void ** data);
bool corpc_unpack_sm_sendfile_chunk(struct sm_sendfile_chunk * sm_sendfile_chunk, const void * data, size_t size);

ssize_t corpc_pack_sm_timer_event(const struct sm_timer_event * sm_timer_event, void ** data);
bool corpc_unpack_sm_timer_event(struct sm_timer_event * sm_timer_event, const void * data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* __tests_corpc_callback_proto_smaster_pb_h__ */
