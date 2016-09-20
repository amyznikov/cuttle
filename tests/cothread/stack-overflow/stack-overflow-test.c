/*
 * stack-overflow-test.c
 *
 *  Created on: Sep 19, 2016
 *      Author: amyznikov
 */

#include <cuttle/debug.h>
#include <cuttle/daemon.h>
#include <cuttle/cothread/scheduler.h>
#include <unistd.h>

#define TEST_STACK_SIZE  (256*1024)
#define TEST_BUFF_SIZE   (128*1024)


static void thread_proc(void * arg )
{
  const char * myname = arg;

  char buf[TEST_BUFF_SIZE] = "";

  CF_DEBUG("[%s] enter", myname);


  for ( int i = 0; i < 100; ++i ) {
    CF_DEBUG("[%s] memset", myname);
    memset(buf, 0, sizeof(buf));
    CF_DEBUG("[%s] co_yield()", myname);
    co_sleep(1000);
  }

  CF_DEBUG("[%s] leave", myname);
}




int main(int argc, char *argv[])
{
  const char * tasks[] = {
    "task1", "task2", "task3", "task4"
  };

  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);

  if ( !co_scheduler_init(1) ) {
    CF_CRITICAL("co_scheduler_init() fails");
    return 1;
  }


  if ( !cf_setup_signal_handler() ) {
    CF_CRITICAL("cf_setup_signal_handler() fails");
  }


  for ( size_t i = 0; i < sizeof(tasks)/sizeof(tasks[0]); ++i ) {
    if ( !co_schedule(thread_proc, (void*)tasks[i], TEST_STACK_SIZE) ) {
      CF_CRITICAL("co_schedule(thread_proc) fails: %s", strerror(errno));
      return 1;
    }
  }

  while ( 42 ) {
    sleep(1);
  }


  return 0;
}
