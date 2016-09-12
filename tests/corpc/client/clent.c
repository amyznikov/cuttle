/*
 * clent.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#include <stdio.h>
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

//////////////////



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

  st = corpc_open_stream(channel,
      &(const corpc_open_stream_opts ) {
            .service = "auth",
            .method = "authenicate"
          });

  if ( !st ) {
    goto end;
  }

  auth_request.iam = "Vasya";
  if ( !(fok = corpc_stream_send_auth_request(st, &auth_request)) ) {
    goto end;
  }

  if ( !(fok = corpc_stream_recv_auth_cookie(st, &auth_cookie)) ) {
    goto end;
  }

  // generate signature
  memcpy(auth_cookie_sign.sign, auth_cookie.cookie, sizeof(auth_cookie_sign.sign));

  if ( !(fok = corpc_stream_send_auth_cookie_sign(st, &auth_cookie_sign)) ) {
    goto end;
  }

  if ( !(fok = corpc_stream_recv_auth_responce(st, &auth_responce)) ) {
    goto end;
  }

  // process responce
  printf("%p\n", auth_responce.ticket);

end:

  corpc_close_stream(st);

  return fok;
}


//////////////////


bool get_mail(corpc_channel * channel)
{
  corpc_stream * st = NULL;
  struct mail mail;

  mail_init(&mail);

  st = corpc_open_stream(channel,
      &(const corpc_open_stream_opts ) {
        .service = "mailbox",
        .method = "getmail"
      });

  if ( !st ) {
    goto end;
  }

  while ( corpc_stream_read_mail(st, &mail) ) {
    mail_cleanup(&mail);
  }

end:

  corpc_close_stream(st);

  mail_cleanup(&mail);

  return true;
}

//////////////////

int main(/*int argc, char *argv[]*/)
{
  corpc_channel * channel;

  channel = corpc_channel_new(&(struct corpc_channel_opts ) {
        .connect_address = "crkdev1.special-is.com",
        .connect_port = 6008,
        .ssl_ctx = NULL,
        .onstatechanged = on_channel_state_changed,
      });

  if ( !channel ) {
    goto end;
  }

  if ( !corpc_open_channel(channel) ) {
    goto end;
  }

  if ( !get_mail(channel) ) {
    goto end;
  }


end:

  corpc_channel_relase(&channel);


  return 0;
}
