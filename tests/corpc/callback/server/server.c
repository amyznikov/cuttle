/*
 * server.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#include <stdio.h>
#include <unistd.h>

#include <cuttle/debug.h>
#include <cuttle/sockopt.h>
#include <cuttle/ssl/init-ssl.h>
#include <cuttle/corpc/corpc-server.h>

#include "../proto/smaster.h"

typedef
struct client_context {
  corpc_channel * channel; // save for event notifications
} client_context;


static bool event_sender_finished = false;
static bool event_receiver_finished = false;


///////////////////////////////////////////////////////////////////////////////////////////

static void send_timer_events_to_client(corpc_channel * channel)
{
  corpc_stream * st = NULL;
  sm_timer_event event;
  int i = 0;

  CF_DEBUG("STARTED");

  init_sm_timer_event(&event, "SERVER-TIMER-EVENT");

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
    co_sleep(750);
  }

end:

  corpc_close_stream(&st);

  cleanup_sm_timer_event(&event);
  event_sender_finished = true;
  CF_DEBUG("FINISHED");
}


///////////////////////////////////////////////////////////////////////////////////////////
static bool on_accept_client_connection(const corpc_channel * channel)
{
  CF_NOTICE("ACCEPTED NEW CLIENT CONNECTION");
  return true;
}

///////////////////////////////////////////////////////////////////////////////////////////

static void on_accepted_client_connection(corpc_channel * channel)
{
  send_timer_events_to_client(channel);
}

///////////////////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////////////////

static void receive_timer_events_from_client(corpc_stream * st)
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

static corpc_service client_event_listener_service = {
  .name = k_smaster_events_service_name,
  .methods = {
    { .name = k_smaster_events_ontimer_methd_name, .proc = receive_timer_events_from_client },
    { .name = NULL },
  }
};


static const corpc_service * server_services[] = {
  &client_event_listener_service,
  NULL
};

///////////////////////////////////////////////////////////////////////////////////////////


int main(/*int argc, char *argv[]*/)
{
  corpc_server * server = NULL;
  bool fok = false;


  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);

  CF_DEBUG("C cf_ssl_initialize()");

  if ( !cf_ssl_initialize() ) {
    CF_FATAL("cf_ssl_initialize() fails");
    goto end;
  }



  CF_DEBUG("C co_scheduler_init()");
  if ( !co_scheduler_init(4) ) {
    CF_FATAL("co_scheduler_init() fails");
    goto end;
  }


  CF_DEBUG("C corpc_server_new()");
  server = corpc_server_new(
      &(struct corpc_server_opts ) {
        .ssl_ctx = NULL
      });

  if ( !server ) {
    CF_FATAL("corpc_server_new() fails");
    goto end;
  }

  CF_DEBUG("C corpc_server_add_port()");

  fok = corpc_server_add_port(server,
      &(struct corpc_listening_port_opts ) {

          .listen_address.in = {
            .sin_family = AF_INET,
            .sin_port = htons(6008),
            .sin_addr.s_addr = 0,
          },

          .services = server_services,

          .onaccept = on_accept_client_connection,
          .onaccepted = on_accepted_client_connection,

          });


  if ( !fok ) {
    CF_FATAL("corpc_server_add_port() fails");
    goto end;
  }

  CF_DEBUG("C corpc_server_start()");
  if ( !(fok = corpc_server_start(server)) ) {
    CF_FATAL("corpc_server_start() fails");
    goto end;
  }

  CF_DEBUG("Server started");

  while ( 42 ) {
    sleep(1);
  }

end:

  CF_DEBUG("Finished");
  return 0;
}
