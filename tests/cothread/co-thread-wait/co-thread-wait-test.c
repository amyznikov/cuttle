/*
 * co-thread-wait-test.c
 *
 *  Created on: Sep 14, 2016
 *      Author: amyznikov
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <cuttle/debug.h>
#include <cuttle/cothread/scheduler.h>

static co_thread_lock_t * mtx;

static int some_data;

static bool finished = false;

static void producer(void * arg)
{
  const char * myname = arg;
  int nb_signalled;

  CF_INFO("[%s] Started", myname);

  if ( !co_thread_lock(&mtx) ) {
    CF_CRITICAL("co_thread_wait_lock() fails: %s", strerror(errno));
    goto end;
  }

  for ( int i = 0; i < 1000; ++i ) {

    some_data = i;

//    if ( (nb_signalled = co_thread_signal(&mtx)) < 0 ) {
//      CF_CRITICAL("co_thread_wait_signal() fails: %s", strerror(errno));
//      goto end;
//    }

    if ( (nb_signalled = co_thread_broadcast(&mtx)) < 0 ) {
      CF_CRITICAL("co_thread_wait_broadcast() fails: %s", strerror(errno));
      goto end;
    }

    if ( !co_thread_unlock(&mtx) ) {
      CF_CRITICAL("co_thread_wait_unlock() fails: %s", strerror(errno));
      goto end;
    }

    //co_sleep(1000);

    if ( !co_thread_lock(&mtx) ) {
      CF_CRITICAL("co_thread_wait_lock() fails: %s", strerror(errno));
      goto end;
    }
  }

end :
  if ( !co_thread_unlock(&mtx) ) {
    CF_CRITICAL("co_thread_wait_unlock() fails: %s", strerror(errno));
  }

  CF_INFO("[%s] Finished", myname);

  finished = true;
}

static void consumer(void * arg)
{
  const char * myname = arg;
  int status;

  int oldv = 0;

  CF_INFO("[%s] Started", myname);

  if ( !co_thread_lock(&mtx) ) {
    CF_CRITICAL("co_thread_wait_lock() fails: %s", strerror(errno));
    goto end;
  }


  while ( 32 ) {

    if ( (status = co_thread_wait(&mtx, -1)) < 0 ) {
      CF_CRITICAL("co_thread_wait() fails: %s", strerror(errno));
      goto end;
    }

    if ( status == 0 ) {
      CF_ERROR("[%s] timeout", myname);
    }
    else {

      if ( some_data != oldv + 1) {
        CF_CRITICAL("[%s] NOT EQUAL: some_data=%d oldv=%d", myname, some_data, oldv );
      }
      else {
        CF_NOTICE("[%s] : %d", myname, some_data);
      }

      oldv = some_data;

    }
  }


end :

if ( !co_thread_unlock(&mtx) ) {
    CF_CRITICAL("co_thread_wait_unlock() fails: %s", strerror(errno));
  }

  CF_INFO("[%s] Finished", myname);
}

int main(/*int argc, char *argv[]*/)
{

  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);

  CF_DEBUG("=======================\n");
  if ( !co_scheduler_init(4) ) {
    CF_FATAL("co_scheduler_init() fails");
    goto end;
  }

  if ( !co_schedule(consumer, "consumer-1", 1024 * 1024) ) {
    CF_FATAL("co_schedule(consumer) fails");
    goto end;
  }

  if ( !co_schedule(consumer, "consumer-2", 1024 * 1024) ) {
    CF_FATAL("co_schedule(consumer) fails");
    goto end;
  }

  if ( !co_schedule(consumer, "consumer-3", 1024 * 1024) ) {
    CF_FATAL("co_schedule(consumer) fails");
    goto end;
  }

  if ( !co_schedule(producer, "producer", 1024 * 1024) ) {
    CF_FATAL("co_schedule(producer) fails");
    goto end;
  }


  while ( !finished ) {
    usleep(10*1000);
  }

end :

  CF_DEBUG("=======================\n\n");

  return 0;
}
