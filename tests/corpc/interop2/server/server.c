/*
 * server.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#include <cuttle/corpc/server.h>
#include <stdio.h>
#include <unistd.h>

#include <cuttle/debug.h>
#include <cuttle/sockopt.h>
#include <cuttle/ssl/init-ssl.h>
#include <cuttle/ccarray.h>
#include "../proto/auth.h"
#include "../proto/mail.h"
#include "../proto/resource-request.h"



typedef
struct client_context {
  corpc_channel * channel; // save for event notifications
  char cid[64];
} client_context;


static ccarray_t g_clients_list;



static struct client_context * get_client_context(const char * cid)
{
  struct client_context * cc;
  size_t i, n;
  for ( i = 0, n = ccarray_size(&g_clients_list); i < n; ++i ) {
    if ( strcmp((cc = ccarray_ppeek(&g_clients_list, i))->cid, cid) == 0 ) {
      return cc;
    }
  }
  return NULL;
}






static client_context * client_context_new(corpc_channel * channel)
{
  client_context * ctx;

  if ( (ctx = calloc(1, sizeof(struct client_context))) ) {
    ctx->channel = channel;
  }

  return ctx;
}


static void on_client_accepted(corpc_channel * channel)
{
  client_context * ctx = NULL;
  if ( (ctx = client_context_new(channel)) ) {
    corpc_channel_set_client_context(channel, ctx);
  }
}

static void on_client_disconnected(corpc_channel * channel)
{
  client_context * cc;
  CF_DEBUG("on_client_disconnected");
  if ( (cc = corpc_channel_get_client_context(channel)) ) {
    CF_DEBUG("ccarray_erase_item(%s)", cc->cid);
    ccarray_erase_item(&g_clients_list, &cc);
  }
}


static void on_smaster_authenticate(corpc_stream * st)
{
  struct client_context * cli;
  struct auth_request auth_request;
  struct auth_cookie auth_cookie;
  struct auth_cookie_sign auth_cookie_sign;
  struct auth_responce auth_responce;


  CF_DEBUG("ENTER");

  if ( !(cli = corpc_stream_get_channel_client_context(st)) ) {
    CF_CRITICAL("corpc_stream_get_channel_client_context() fails");
    goto end;
  }

  if ( !corpc_stream_read_auth_request(st, &auth_request) ) {
    CF_CRITICAL("corpc_stream_read_auth_request() fails");
    goto end;
  }

  CF_DEBUG("GOT auth_request='%s'", auth_request.text);

  auth_cookie.text = strdup("THIS IS AUTH COOKIE");
  if ( !corpc_stream_write_auth_cookie(st, &auth_cookie) ) {
    CF_CRITICAL("corpc_stream_write_auth_cookie() fails");
    goto end;
  }

  if ( !corpc_stream_read_auth_cookie_sign(st, &auth_cookie_sign)) {
    CF_CRITICAL("corpc_stream_read_auth_cookie_sign() fails");
    goto end;
  }

  CF_DEBUG("GOT auth_cookie_sign='%s'", auth_cookie_sign.text);

  auth_responce.text = strdup("THIS IS AUTH_RESPONCE");
  if ( !corpc_stream_write_auth_responce(st, &auth_responce)) {
    CF_CRITICAL("corpc_stream_write_auth_responce() fails");
    goto end;
  }

  strncpy(cli->cid, auth_request.text, sizeof(cli->cid) - 1);
  ccarray_push_back(&g_clients_list, &cli);

end:

  CF_DEBUG("LEAVE");

  return;
}

static void mailbox_get_mail(corpc_stream * st)
{
  //struct client_context * cli;
  struct mail mail;
  bool fok = true;

  for ( int i = 0; i < 10 && fok; ++i ) {

    char buf[256];
    sprintf(buf, "mail-%d", i);

    co_sleep(1000);

    mail_init(&mail,
        &(struct mail_init_args ) {
              .text = buf
            });

    if ( !(fok = corpc_stream_write_mail(st, &mail)) ) {
      CF_CRITICAL("corpc_stream_write_mail() fails");
    }

    mail_cleanup(&mail);
  }

  CF_DEBUG("Finished");
  return;
}


static void resource_service_get_resource(corpc_stream * st)
{
  corpc_stream * st2 = NULL;

  struct resource_request request = {
    .resource_id = NULL
  };

  struct resource_request_responce responce = {
    .resource_data = NULL
  };

  struct client_context * cc = NULL;

  CF_DEBUG("////////////////////////////////////////////////////////////////////////////");

  CF_DEBUG("corpc_stream_read_resource_request()");

  if ( !corpc_stream_read_resource_request(st, &request) ) {
    CF_FATAL("corpc_stream_read_resource_request() fails");
    goto __end;
  }


  CF_DEBUG("get_client_context()");
  if ( !(cc = get_client_context(request.resource_id)) ) {
    CF_FATAL("requested resource '%s' not available", request.resource_id);
    goto __end;
  }


  CF_DEBUG("st2 = corpc_open_stream()");
  st2 = corpc_open_stream(cc->channel, &(corpc_open_stream_opts ) {
        .service = "get_resource",
        .method = "get_resource"
      });

  if ( !st2 ) {
    CF_FATAL("corpc_open_stream(get_resource) fails");
    goto __end;
  }

  CF_DEBUG("corpc_stream_write_resource_request(st2)");
  if ( !corpc_stream_write_resource_request(st2, &request) ) {
    CF_FATAL("corpc_stream_write_resource_request(st2) fails");
    goto __end;
  }

  CF_DEBUG("corpc_stream_read_resource_request_responce(st2)");
  if ( !corpc_stream_read_resource_request_responce(st2, &responce) ) {
    CF_FATAL("corpc_stream_read_resource_request_responce(st2) fails");
    goto __end;
  }

__end:;

  if ( responce.resource_data == NULL ) {
    responce.resource_data = strdup("RRR FAIL");
  }

  if ( responce.resource_data != NULL ) {
    CF_DEBUG("corpc_stream_write_resource_request_responce(st, '%s')", responce.resource_data);
    if ( !corpc_stream_write_resource_request_responce(st, &responce) ) {
      CF_FATAL("corpc_stream_write_resource_request_responce(st) fails");
    }
  }

  free(request.resource_id);
  free(responce.resource_data);

  CF_DEBUG("////////////////////////////////////////////////////////////////////////////");
}





void send_some_smaster_event_notify(client_context * cli)
{
  corpc_msg msg;
  corpc_stream * st;

  corpc_msg_init(&msg);

  st = corpc_open_stream(cli->channel,
      &(const corpc_open_stream_opts ) {
            .service = "smaster.events",
            .method = "on_some_state_changed",
          });

  if ( st ) {
    corpc_stream_write(st, msg.data, msg.size);
    corpc_close_stream(&st);
  }

  corpc_msg_clean(&msg);
}




int main(/*int argc, char *argv[]*/)
{
  corpc_server * server = NULL;
  bool fok = false;


  static corpc_service smaster_service = {
    .name = "auth",
    .methods = {
      { .name = "authenicate", .proc = on_smaster_authenticate },
      { .name = NULL },
    }
  };

  static corpc_service mailbox_service = {
    .name = "mailbox",
    .methods = {
      { .name = "getmail", .proc = mailbox_get_mail},
      { .name = NULL },
    }
  };

  static corpc_service resource_service = {
    .name = "resource",
    .methods = {
      { .name = "get_resource", .proc = resource_service_get_resource},
      { .name = NULL },
    }
  };


  static const corpc_service * my_services[] = {
    &smaster_service,
    &mailbox_service,
    &resource_service,
    NULL
  };



  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);


  ccarray_init(&g_clients_list, 1000, sizeof(client_context*));

  CF_DEBUG("C cf_ssl_initialize()");

  if ( !cf_ssl_initialize() ) {
    CF_FATAL("cf_ssl_initialize() fails");
    goto end;
  }



  CF_DEBUG("C co_scheduler_init()");
  if ( !co_scheduler_init(2) ) {
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

            .services = my_services,

            .onaccepted = on_client_accepted,

            .ondisconnected = on_client_disconnected
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
