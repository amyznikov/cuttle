/*
 * smaster.pb.c
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */


#include "smaster.pb.h"
#include <string.h>


ssize_t corpc_pack_sm_timer_event(const struct sm_timer_event * msg, void ** data)
{
  return strlen(*data = strdup(msg->msg)) + 1;
}

bool corpc_unpack_sm_timer_event(struct sm_timer_event * msg, const void * data, size_t size)
{
  (void)(size);
  return ((msg->msg = strdup(data)) != NULL);
}
