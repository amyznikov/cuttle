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
#include <cuttle/corpc/channel.h>
#include "../proto/auth.h"
#include "../proto/mail.h"


//////////////////

static void on_channel_state_changed(corpc_channel * channel, enum corpc_channel_state state, int reason)
{
  (void)channel;
  (void)state;
  (void)reason;
}

////////////////////////////////////////////////////////////////////////////////////////////////////



bool authenticate(corpc_channel * channel, const char * iam, const char * password)
{
  (void)(password);
  (void)(iam);

  struct auth_request auth_request;
  struct auth_cookie auth_cookie;
  struct auth_cookie_sign auth_cookie_sign;
  struct auth_responce auth_responce;

  corpc_stream * st = NULL;
  bool fok = false;

  CF_DEBUG("ENTER");

  st = corpc_open_stream(channel,
      &(const corpc_open_stream_opts ) {
            .service = "auth",
            .method = "authenicate"
          });

  if ( !st ) {
    CF_CRITICAL("corpc_open_stream() fails");
    goto end;
  }

  auth_request.text = strdup("My Name is Vasya");
  if ( !(fok = corpc_stream_write_auth_request(st, &auth_request)) ) {
    CF_CRITICAL("corpc_stream_write_auth_request() fails");
    goto end;
  }

  if ( !(fok = corpc_stream_read_auth_cookie(st, &auth_cookie)) ) {
    CF_CRITICAL("corpc_stream_read_auth_cookie() fails");
    goto end;
  }

  CF_DEBUG("GOT auth_cookie: '%s'", auth_cookie.text);


  // generate signature
  auth_cookie_sign.text = strdup("THIS IS AUTH COOKIE SIGN");

  if ( !(fok = corpc_stream_write_auth_cookie_sign(st, &auth_cookie_sign)) ) {
    CF_CRITICAL("corpc_stream_write_auth_cookie_sign() fails");
    goto end;
  }

  if ( !(fok = corpc_stream_read_auth_responce(st, &auth_responce)) ) {
    CF_CRITICAL("corpc_stream_read_auth_responce() fails");
    goto end;
  }

  CF_DEBUG("GOT auth_responce: '%s'", auth_responce.text);

end:

  corpc_close_stream(&st);

  CF_DEBUG("LEAVE: fok= %d", fok);
  return fok;
}


////////////////////////////////////////////////////////////////////////////////////////////////////


bool get_mail(corpc_channel * channel)
{
  corpc_stream * st = NULL;
  struct mail mail;

  mail_init(&mail, NULL);

  st = corpc_open_stream(channel,
      &(const corpc_open_stream_opts ) {
        .service = "mailbox",
        .method = "getmail"
      });

  if ( !st ) {
    CF_CRITICAL("corpc_open_stream() fails");
    goto end;
  }

  CF_INFO("BEGIN READ MAILS");

  while ( corpc_stream_read_mail(st, &mail) ) {

    CF_INFO("MAIL: %s", mail.text);

    mail_cleanup(&mail);
  }

  CF_INFO("END READ MAILS");

end:

  CF_DEBUG("C corpc_close_stream()");
  corpc_close_stream(&st);
  CF_DEBUG("R corpc_close_stream()");

  mail_cleanup(&mail);

  return true;
}


//////////////////

static void client_main(void * arg )
{
  (void) arg;

  corpc_channel * channel;

  CF_DEBUG("Started");


  channel = corpc_channel_new(&(struct corpc_channel_opts ) {
        .connect_address = "localhost",
        .connect_port = 6008,
        .ssl_ctx = NULL,
        .onstatechanged = on_channel_state_changed,
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

  if ( !authenticate(channel, "iam", "password") ) {
    CF_FATAL("authenticate() fails");
    goto end;
  }

  if ( !get_mail(channel) ) {
    CF_FATAL("corpc_open_channel() fails");
    goto end;
  }


end:


  CF_DEBUG("C corpc_channel_relase()");
  corpc_channel_close(&channel);

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

  while ( 42 ) {
    sleep(1);
  }

end:

  return 0;
}
