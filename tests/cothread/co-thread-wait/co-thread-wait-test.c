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
#include <pthread.h>
#include <cuttle/debug.h>
#include <cuttle/cothread/scheduler.h>


static co_thread_lock_t mtx;

static int some_data;

static bool finished = false;

static void producer(void * arg)
{
  const char * myname = arg;
  int nb_signalled;

  CF_INFO("[%s] Started", myname);

  if ( !co_thread_lock(&mtx) ) {
    CF_CRITICAL("co_thread_lock() fails: %s", strerror(errno));
    goto end;
  }

  for ( int i = 0; i < 1000; ++i ) {

    some_data = i;

//    if ( (nb_signalled = co_thread_signal(&mtx)) < 0 ) {
//      CF_CRITICAL("co_thread_wait_signal() fails: %s", strerror(errno));
//      goto end;
//    }

    if ( (nb_signalled = co_thread_broadcast(&mtx)) < 0 ) {
      CF_CRITICAL("co_thread_broadcast() fails: %s", strerror(errno));
      goto end;
    }

    if ( !co_thread_unlock(&mtx) ) {
      CF_CRITICAL("co_thread_unlock() fails: %s", strerror(errno));
      goto end;
    }

    //co_sleep(1000);

    if ( !co_thread_lock(&mtx) ) {
      CF_CRITICAL("co_thread_lock() fails: %s", strerror(errno));
      goto end;
    }
  }

end :
  if ( !co_thread_unlock(&mtx) ) {
    CF_CRITICAL("co_thread_unlock() fails: %s", strerror(errno));
  }

  CF_INFO("[%s] Finished", myname);

  finished = true;
}


static void * pth_producer(void * arg)
{
  pthread_detach(pthread_self());
  producer(arg);
  return NULL;
}



static void co_consumer(void * arg)
{
  const char * myname = arg;
  int status;

  int oldv = 0;

  CF_INFO("[%s] Started", myname);

  if ( !co_thread_lock(&mtx) ) {
    CF_CRITICAL("co_thread_lock() fails: %s", strerror(errno));
    goto end;
  }

  while ( 32 ) {

    if ( (status = co_thread_wait(&mtx, -1)) < 0 ) {
      CF_CRITICAL("co_thread_wait() fails: %s", strerror(errno));
    }
    else if ( status == 0 ) {
      CF_ERROR("[%s] timeout", myname);
    }
    else {
      CF_NOTICE("[%s] : %d", myname, some_data);
      oldv = some_data;
    }
  }


end :

  if ( !co_thread_unlock(&mtx) ) {
    CF_CRITICAL("co_thread_unlock() fails: %s", strerror(errno));
  }

  CF_INFO("[%s] Finished", myname);
}


static void * pth_consumer(void * arg)
{
  const char * myname = arg;

  pthread_detach(pthread_self());

  CF_DEBUG("PTH [%s] STARTED", myname);

  co_consumer(arg);

  CF_DEBUG("PTH [%s] FINISHED", myname);

  return NULL;
}



int main(/*int argc, char *argv[]*/)
{

  static const char * co_consumers[] = {
    "co-consumer-0",
    "co-consumer-1",
  };

  static const char * pth_consumers[] = {
    "pth-consumer-0",
    "pth-consumer-1",
  };

  pthread_t pid;
  int status;

  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);

  CF_DEBUG("=======================\n");
  if ( !co_scheduler_init(4) ) {
    CF_FATAL("co_scheduler_init() fails");
    goto end;
  }


  for ( size_t i = 0; i < sizeof(co_consumers) / sizeof(co_consumers[0]); ++i ) {
    if ( !co_schedule(co_consumer, (void*)co_consumers[i], 0) ) {
      CF_FATAL("co_schedule(%s) fails: %s", co_consumers[i], strerror(errno));
      goto end;
    }
  }
  for ( size_t i = 0; i < sizeof(pth_consumers) / sizeof(pth_consumers[0]); ++i ) {
    if ( (status = pthread_create(&pid, NULL, pth_consumer, (void*)pth_consumers[i])) ) {
      CF_FATAL("pthread_create(%s) fails: %s", pth_consumers[i], strerror(status));
      goto end;
    }
  }


  if ( true ) {
    if ( !co_schedule(producer, "co-producer", 0) ) {
      CF_FATAL("co_schedule(co-producer) fails");
      goto end;
    }
  }
  else {
    if ( (status = pthread_create(&pid, NULL, pth_producer, (void*) "pth-producer")) ) {
      CF_FATAL("pthread_create(pth_producer) fails: %s", strerror(status));
      goto end;
    }
  }


  while ( !finished ) {
    usleep(10*1000);
  }

end :

  CF_DEBUG("=======================\n\n");

  return 0;
}
