/*
 * clent.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#include <stdio.h>
#include <unistd.h>
#include <cuttle/debug.h>
#include <cuttle/ssl/init-ssl.h>

#include "../proto/smaster.h"



static bool event_sender_finished = false;
static bool event_receiver_finished = false;

/////////////////////////////////////////////////////////////////////////////////////////////

static void send_timer_events_to_server(void * arg)
{
  corpc_channel * channel = arg;
  corpc_stream * st = NULL;
  sm_timer_event event;
  int i = 0;

  CF_DEBUG("STARTED");

  init_sm_timer_event(&event, "CLIENT-TIMER-EVENT");

  st = corpc_open_stream(channel,
      &(struct corpc_open_stream_opts ) {
            .service = k_smaster_events_service_name,
            .method = k_smaster_events_ontimer_methd_name,
          });

  if ( !st ) {
    CF_CRITICAL("corpc_open_stream() fails");
    goto end;
  }

  while ( i++ < 10000 && corpc_stream_write_sm_timer_event(st, &event) ) {
    co_sleep(500);
  }

end:

  corpc_close_stream(&st);

  cleanup_sm_timer_event(&event);
  event_sender_finished = true;
  CF_DEBUG("FINISHED");
}

/////////////////////////////////////////////////////////////////////////////////////////////

static void receive_timer_events_from_server(corpc_stream * st)
{
  sm_timer_event e;
  CF_DEBUG("ENTER");

  init_sm_timer_event(&e, NULL);

  while ( corpc_stream_read_sm_timer_event(st, &e))  {
    CF_DEBUG("%s", e.msg);
    cleanup_sm_timer_event(&e);
  }

  CF_DEBUG("corpc_stream_read_sm_timer_event() fails");

  cleanup_sm_timer_event(&e);

  event_receiver_finished = true;
  CF_DEBUG("LEAVE");
}

static const corpc_service server_event_listener_service = {
  .name = k_smaster_events_service_name,
  .methods = {
    { .name = k_smaster_events_ontimer_methd_name, .proc = receive_timer_events_from_server },
    { .name = NULL },
  }
};

static const corpc_service * client_services[] = {
  &server_event_listener_service,
  NULL
};


/////////////////////////////////////////////////////////////////////////////////////////////

static void client_main(void * arg )
{
  (void) arg;

  corpc_channel * channel;

  CF_DEBUG("Started");


  channel = corpc_channel_new(&(struct corpc_channel_opts ) {
        .connect_address = "localhost",
        .connect_port = 6008,
        .services = client_services,
        .ssl_ctx = NULL,
        .onstatechanged = NULL,
      });

  if ( !channel ) {
    CF_FATAL("corpc_channel_new() fails");
    goto end;
  }

  CF_DEBUG("channel->state = %s", corpc_channel_state_string(corpc_get_channel_state(channel)));

  if ( !corpc_channel_open(channel) ) {
    CF_FATAL("corpc_open_channel() fails: %s", strerror(errno));
    goto end;
  }


  if ( !co_schedule(send_timer_events_to_server, channel, 1024 * 1024) ) {
    CF_FATAL("co_schedule(start_timer_events) fails: %s", strerror(errno));
    goto end;
  }

end:

//  CF_DEBUG("C corpc_channel_relase()");
//  corpc_channel_close(channel);

  CF_DEBUG("Finished");
}


//////////////////

int main(/*int argc, char *argv[]*/)
{

  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);


  if ( !cf_ssl_initialize() ) {
    CF_FATAL("cf_ssl_initialize() fails: %s", strerror(errno));
    goto end;
  }

  if ( !co_scheduler_init(2) ) {
    CF_FATAL("co_scheduler_init() fails: %s", strerror(errno));
    goto end;
  }

  if ( !co_schedule(client_main, NULL, 1024 * 1024) ) {
    CF_FATAL("co_schedule(server_thread) fails: %s", strerror(errno));
    goto end;
  }

  while ( !event_sender_finished ) {
    usleep(20 * 1000);
  }

end:

  return 0;
}
