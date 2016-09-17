/*
 * corpc-msg.c
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

#include <malloc.h>
#include <string.h>
#include "corpc-msg.h"


void corpc_msg_init(struct corpc_msg * comsg)
{
  memset(comsg, 0, sizeof(*comsg));
}

void corpc_msg_clean(struct corpc_msg * comsg)
{
  free(comsg->data);
  comsg->data = NULL;
  comsg->size = 0;
}

void corpc_msg_set(struct corpc_msg * comsg, void * data, size_t size)
{
  free(comsg->data);
  comsg->data = data;
  comsg->size = size;
}
