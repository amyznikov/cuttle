/*
 * server.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#include <stdio.h>
#include <unistd.h>
#include <cuttle/corpc/corpc-server.h>
#include "../proto/auth.h"
#include "../proto/mail.h"

typedef
struct client_context {
  corpc_channel * channel; // save for event notifications
} client_context;

static client_context * client_context_new(corpc_channel * channel)
{
  client_context * ctx;

  if ( (ctx = calloc(1, sizeof(struct client_context))) ) {
    ctx->channel = channel;
  }

  return ctx;
}


static bool on_client_accepted(corpc_channel * channel)
{
  client_context * ctx = NULL;

  bool fok = false;

  if ( (ctx = client_context_new(channel)) ) {
    corpc_channel_set_client_context(channel, ctx);
    fok = true;
  }

  return fok;
}

static void on_client_disconnected(corpc_channel * channel)
{
  (void)(channel);
}


static void on_smaster_authenticate(corpc_stream * st)
{
  struct client_context * cli;
  struct auth_request auth_request;
  struct auth_cookie auth_cookie;
  struct auth_cookie_sign auth_cookie_sign;
  struct auth_responce auth_responce;

  if ( !(cli = corpc_stream_get_channel_client_context(st)) ) {
    // something bugged
    goto end;
  }

  if ( !corpc_stream_read_auth_request(st, &auth_request) ) {
    // something wrong
    goto end;
  }

  if ( !corpc_stream_write_auth_cookie(st, &auth_cookie) ) {
    // something wrong
    goto end;
  }

  if ( !corpc_stream_read_auth_cookie_sign(st, &auth_cookie_sign)) {
    // something wrong
    goto end;
  }

  if ( !corpc_stream_write_auth_responce(st, &auth_responce)) {
    // something wrong
    goto end;
  }

end:

  return;
}

static void on_smaster_get_mail(corpc_stream * st)
{
  struct client_context * cli;
  struct mail mail;

  mail_init(&mail);

  if ( !(cli = corpc_stream_get_channel_client_context(st)) ) {
    // something wrong
  }
  else {
    corpc_stream_write_mail(st, &mail);
  }

  mail_cleanup(&mail);

  return;
}


static corpc_service smaster_service = {
  .name = "smaster",
  .methods = {
    { .name = "authenticate", .proc = on_smaster_authenticate },
    { .name = "get-mail", .proc = on_smaster_get_mail},
    { .name = NULL },
  }
};


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
    corpc_stream_write(st, &msg);
    corpc_close_stream(&st);
  }

  corpc_msg_clean(&msg);
}




int main(/*int argc, char *argv[]*/)
{
  corpc_server * server = NULL;
  bool fok = false;

  server = corpc_server_new(
      &(struct corpc_server_opts ) {
        .ssl_ctx = NULL
      });

  if ( !server ) {
    goto end;
  }

  fok = corpc_server_add_port(server,
      &(struct corpc_listening_port_opts ) {

            .listen_address.in = {
              .sin_family = AF_INET,
              .sin_port = htons(6008),
              .sin_addr.s_addr = 0,
            },

            .services = (const corpc_service *[] ) {
              &smaster_service
            },
            .nb_services = 1,

            .onaccepted = on_client_accepted,
            .ondisconnected = on_client_disconnected,

          });


  if ( !fok ) {
    goto end;
  }

  if ( !(fok = corpc_server_start(server)) ) {
    goto end;
  }

  while ( 42 ) {
    sleep(1);
  }

end:

  return 0;
}
