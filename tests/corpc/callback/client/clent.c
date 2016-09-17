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



static bool finished_timer = false;
static bool finished_sender = false;

/////////////////////////////////////////////////////////////////////////////////////////////

static void start_timer_events(void * arg)
{
  corpc_channel * channel = arg;
  corpc_stream * st = NULL;
  sm_timer_event event;
  int i = 0;

  CF_DEBUG("STARTED");

  init_sm_timer_event(&event, "ON-TIMER-EVENT");

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
    co_sleep(1);
  }

end:

  corpc_close_stream(&st);

  cleanup_sm_timer_event(&event);
  finished_timer = true;
  CF_DEBUG("FINISHED");
}


/////////////////////////////////////////////////////////////////////////////////////////////

static void send_file_to_server(void * arg)
{
  corpc_channel * channel = arg;
  corpc_stream * st = NULL;
  sm_sendfile_request req;
  sm_sendfile_responce resp;
  sm_sendfile_chunk chunk;

  CF_DEBUG("STARTED");

  init_sm_sendfile_request(&req, "THIS IS FILE NAME");
  init_sm_sendfile_responce(&resp, NULL);
  init_sm_sendfile_chunk(&chunk, "THIS IS FILE CHUNK");


  st = corpc_open_stream(channel,
      &(struct corpc_open_stream_opts ) {
            .service = k_smaster_service_name,
            .method = k_smaster_sendfile_method_name,
          });

  if ( !st ) {
    CF_CRITICAL("corpc_open_stream() fails: %s", strerror(errno));
    goto end;
  }

  if ( !corpc_stream_write_sm_sendfile_request(st, &req) ) {
    CF_CRITICAL("corpc_stream_write_sm_sendfile_request() fails: %s", strerror(errno));
    goto end;
  }


  if ( !corpc_stream_read_sm_sendfile_responce(st, &resp) ) {
    CF_CRITICAL("corpc_stream_read_sm_sendfile_responce() fails: %s", strerror(errno));
    goto end;
  }

  CF_DEBUG("responce: '%s'", resp.resp);

  for ( int i = 0; i < 10; ++i ) {
    if ( !corpc_stream_write_sm_sendfile_chunk(st, &chunk) ) {
      CF_CRITICAL("corpc_stream_write_sm_sendfile_chunk() fails: %s", strerror(errno));
      break;
    }
    co_sleep(100);
  }


end:

  corpc_close_stream(&st);

  cleanup_sm_sendfile_request(&req);
  cleanup_sm_sendfile_responce(&resp);
  cleanup_sm_sendfile_chunk(&chunk);

  CF_DEBUG("FINISHED");
}

/////////////////////////////////////////////////////////////////////////////////////////////

static void client_main(void * arg )
{
  (void) arg;

  corpc_channel * channel;

  CF_DEBUG("Started");


  channel = corpc_channel_new(&(struct corpc_channel_opts ) {
        .connect_address = "localhost",
        .connect_port = 6008,
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


  if ( !co_schedule(start_timer_events, channel, 1024 * 1024) ) {
    CF_FATAL("co_schedule(start_timer_events) fails: %s", strerror(errno));
    goto end;
  }

//  if ( !co_schedule(send_file_to_server, channel, 1024 * 1024) ) {
//    CF_FATAL("co_schedule(send_file_to_server) fails: %s", strerror(errno));
//    goto end;
//  }

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

  while ( !finished_timer ) {
    usleep(20 * 1000);
  }

end:

  return 0;
}
