/*
 * smaster.pb.c
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */


#include "smaster.pb.h"
#include <string.h>


ssize_t corpc_pack_sm_sendfile_request(const struct sm_sendfile_request * msg, void ** data)
{
  return strlen(*data = strdup(msg->filename)) + 1;
}

bool corpc_unpack_sm_sendfile_request(struct sm_sendfile_request * msg, const void * data, size_t size)
{
  (void)(size);
  return ((msg->filename = strdup(data)) != NULL);
}

ssize_t corpc_pack_sm_sendfile_responce(const struct sm_sendfile_responce * msg, void ** data)
{
  return strlen(*data = strdup(msg->resp)) + 1;
}

bool corpc_unpack_sm_sendfile_responce(struct sm_sendfile_responce * msg, const void * data, size_t size)
{
  (void)(size);
  return ((msg->resp = strdup(data)) != NULL);
}

ssize_t corpc_pack_sm_sendfile_chunk(const struct sm_sendfile_chunk * msg, void ** data)
{
  return strlen(*data = strdup(msg->data)) + 1;
}

bool corpc_unpack_sm_sendfile_chunk(struct sm_sendfile_chunk * msg, const void * data, size_t size)
{
  (void)(size);
  return ((msg->data = strdup(data)) != NULL);
}

ssize_t corpc_pack_sm_timer_event(const struct sm_timer_event * msg, void ** data)
{
  return strlen(*data = strdup(msg->msg)) + 1;
}

bool corpc_unpack_sm_timer_event(struct sm_timer_event * msg, const void * data, size_t size)
{
  return ((msg->msg = strdup(data)) != NULL);
}
