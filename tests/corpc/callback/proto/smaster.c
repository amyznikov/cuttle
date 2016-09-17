/*
 * smaster.c
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */

#include "smaster.pb.h"


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

