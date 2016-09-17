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


  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);

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

            .services = (const corpc_service *[] ) {
              &smaster_service,
              &mailbox_service
            },
            .nb_services = 2,

            .onaccepted = on_client_accepted,
            .ondisconnected = on_client_disconnected,

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
