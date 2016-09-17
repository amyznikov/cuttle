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


///////////////////////////////////////////////////////////////////////////////////////////

static void on_smaster_sendfile(corpc_stream * st)
{
  sm_sendfile_request req;
  sm_sendfile_responce resp;
  sm_sendfile_chunk chunk;


  CF_DEBUG("ENTER");

  init_sm_sendfile_request(&req, NULL);
  init_sm_sendfile_responce(&resp, NULL);
  init_sm_sendfile_chunk(&chunk, NULL);

  if ( !corpc_stream_read_sm_sendfile_request(st, &req) ) {
    CF_CRITICAL("corpc_stream_read_sm_sendfile_request() fails");
    goto end;
  }

  resp.resp = strdup("FILENAME OK");
  if ( !corpc_stream_write_sm_sendfile_responce(st, &resp) ) {
    CF_CRITICAL("corpc_stream_write_sm_sendfile_responce() fails");
    goto end;
  }


  while ( corpc_stream_read_sm_sendfile_chunk(st, &chunk)) {
    CF_DEBUG("GOT CHUNK '%s'", chunk.data);
    cleanup_sm_sendfile_chunk(&chunk);
  }

  CF_DEBUG("FILE STREAM FINISHED");


end:
  cleanup_sm_sendfile_request(&req);
  cleanup_sm_sendfile_responce(&resp);
  cleanup_sm_sendfile_chunk(&chunk);

  CF_DEBUG("LEAVE");
}

static void on_client_timer_event(corpc_stream * st)
{
  sm_timer_event e;
  CF_DEBUG("ENTER");

  init_sm_timer_event(&e, NULL);

  while ( corpc_stream_read_sm_timer_event(st, &e))  {
    // CF_DEBUG("%s", e.msg);
    cleanup_sm_timer_event(&e);
  }

  CF_DEBUG("corpc_stream_read_sm_timer_event() fails");

  cleanup_sm_timer_event(&e);

  CF_DEBUG("LEAVE");
}


static corpc_service smaster_service = {
  .name = k_smaster_service_name,
  .methods = {
    { .name = k_smaster_sendfile_method_name, .proc = on_smaster_sendfile },
    { .name = NULL },
  }
};

static corpc_service client_timer_listener_service = {
  .name = k_smaster_events_service_name,
  .methods = {
    { .name = k_smaster_events_ontimer_methd_name, .proc = on_client_timer_event },
    { .name = NULL },
  }
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

            .services = (const corpc_service *[] ) {
              &smaster_service,
              &client_timer_listener_service,
              NULL
            },

            .onaccepted = NULL,
            .ondisconnected = NULL,

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
