/*
 * smaster.c
 *
 *  Created on: Sep 16, 2016
 *      Author: amyznikov
 */

#include "smaster.h"


void init_timer_event(struct timer_event * e, const char * msg)
{
  e->msg = msg ? strdup(msg) : NULL;
}

void cleanup_timer_event(struct timer_event * e)
{
  free(e->msg), e->msg = NULL;
}
