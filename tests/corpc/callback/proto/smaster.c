/*
 * smaster.c
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */

#include "smaster.pb.h"


void init_sm_sendfile_request(struct sm_sendfile_request * msg, const char * fname)
{
  msg->filename = fname ? strdup(fname) : NULL;
}

void cleanup_sm_sendfile_request(struct sm_sendfile_request * msg)
{
  free(msg->filename), msg->filename = NULL;
}

bool corpc_stream_write_sm_sendfile_request(corpc_stream * st, const struct sm_sendfile_request * sm_sendfile_request)
{
  return corpc_stream_write_msg(st, corpc_pack_sm_sendfile_request, sm_sendfile_request);
}

bool corpc_stream_read_sm_sendfile_request(corpc_stream * st, struct sm_sendfile_request * sm_sendfile_request)
{
  return corpc_stream_read_msg(st, corpc_unpack_sm_sendfile_request, sm_sendfile_request);
}



void init_sm_sendfile_responce(struct sm_sendfile_responce * msg, const char * resp)
{
  msg->resp = resp ? strdup(resp) : NULL;
}

void cleanup_sm_sendfile_responce(struct sm_sendfile_responce * msg)
{
  free(msg->resp), msg->resp = NULL;
}

bool corpc_stream_write_sm_sendfile_responce(corpc_stream * st, const struct sm_sendfile_responce * sm_sendfile_responce)
{
  return corpc_stream_write_msg(st, corpc_pack_sm_sendfile_responce,sm_sendfile_responce);
}

bool corpc_stream_read_sm_sendfile_responce(corpc_stream * st, struct sm_sendfile_responce * sm_sendfile_responce)
{
  return corpc_stream_read_msg(st, corpc_unpack_sm_sendfile_responce, sm_sendfile_responce);
}




void init_sm_sendfile_chunk(struct sm_sendfile_chunk * msg, const char * data)
{
  msg->data = data ? strdup(data) : NULL;
}

void cleanup_sm_sendfile_chunk(struct sm_sendfile_chunk * msg)
{
  free(msg->data), msg->data = NULL;
}

bool corpc_stream_write_sm_sendfile_chunk(corpc_stream * st, const struct sm_sendfile_chunk * sm_sendfile_chunk)
{
  return corpc_stream_write_msg(st, corpc_pack_sm_sendfile_chunk, sm_sendfile_chunk);
}

bool corpc_stream_read_sm_sendfile_chunk(corpc_stream * st, struct sm_sendfile_chunk * sm_sendfile_chunk)
{
  return corpc_stream_read_msg(st, corpc_unpack_sm_sendfile_chunk, sm_sendfile_chunk);
}




void init_sm_timer_event(struct sm_timer_event * e, const char * msg)
{
  e->msg = msg ? strdup(msg) : NULL;
}

void cleanup_sm_timer_event(struct sm_timer_event * e)
{
  free(e->msg), e->msg = NULL;
}

bool corpc_stream_write_sm_timer_event(corpc_stream * st, const struct sm_timer_event * e)
{
  return corpc_stream_write_msg(st, corpc_pack_sm_timer_event, e);
}

bool corpc_stream_read_sm_timer_event(corpc_stream * st, struct sm_timer_event * e)
{
  return corpc_stream_read_msg(st, corpc_unpack_sm_timer_event, e);
}

