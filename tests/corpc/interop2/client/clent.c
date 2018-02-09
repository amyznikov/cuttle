/*
 * clent.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <unistd.h>
#include <cuttle/debug.h>
#include <cuttle/ssl/init-ssl.h>
#include <cuttle/corpc/channel.h>
#include "../proto/auth.h"
#include "../proto/mail.h"
#include "../proto/resource-request.h"



static char g_server_address[256] = "127.0.0.1";
static uint16_t g_server_port = 6010;

static char g_client_id[256] = "client1";
static char g_resource_id[256] = "client2";
static bool g_q = true;


static corpc_channel * g_smaster_channel = NULL;


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

  struct request auth_request;
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

  auth_request.text = strdup(g_client_id);
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


bool read_mails(corpc_channel * channel)
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


bool request_smaster_resources(corpc_channel * channel)
{
  corpc_stream * st = NULL;
  int sss;

  while ( 42 ) {

    struct resource_request request = {
      .resource_id = strdup(g_resource_id),
    };

    struct resource_request_responce responce = {
      .resource_data = NULL,
    };


    sss = rand() % 5;
    CF_DEBUG("sleep sss=%d", sss);
    co_sleep(sss * 1000);

    if ( !g_q ) {
      continue;
    }

    CF_DEBUG("corpc_open_stream(get_resource)");

    st = corpc_open_stream(channel,
        &(const corpc_open_stream_opts ) {
          .service = "resource",
          .method = "get_resource"
        });

    if ( !st ) {
      CF_CRITICAL("corpc_open_stream() fails");
      break;
    }

    CF_DEBUG("corpc_stream_write_resource_request()");
    if ( !corpc_stream_write_resource_request(st, &request) ) {
      CF_CRITICAL("corpc_stream_write_resource_request() fails");
      break;
    }

    CF_DEBUG("corpc_stream_read_resource_request_responce()");
    if ( !corpc_stream_read_resource_request_responce(st, &responce) ) {
      CF_CRITICAL("corpc_stream_read_resource_request_responce() fails");
      break;
    }


    CF_DEBUG("RECEIVED RESOURCE DATA: %s", responce.resource_data);

    free(request.resource_id);
    free(responce.resource_data);

    corpc_close_stream(&st);
  }


  corpc_close_stream(&st);

  return true;
}

////////////////////////////////////////////////////////////////////////




static void resource_service_get_resource(corpc_stream * st)
{
  struct resource_request request = {
    .resource_id = NULL
  };

  struct resource_request_responce responce = {
    .resource_data = NULL
  };


  CF_DEBUG("////////////////////////////////////////////////////////////");
  CF_DEBUG("It was called!!!");

  CF_DEBUG("corpc_stream_read_resource_request()");
  if ( !corpc_stream_read_resource_request(st, &request) ) {
    CF_FATAL("corpc_stream_read_resource_request() fails");
    goto __end;
  }

  CF_DEBUG("request.resource_id=%s", request.resource_id);
  asprintf(&responce.resource_data, "RRR TO '%s'", request.resource_id);

  CF_DEBUG("corpc_stream_write_resource_request_responce()");
  if ( !corpc_stream_write_resource_request_responce(st, &responce) ) {
    CF_FATAL("corpc_stream_write_resource_request_responce() fails");
    goto __end;
  }

__end: ;

  free(request.resource_id);
  free(responce.resource_data);

  CF_DEBUG("////////////////////////////////////////////////////////////");
}



////////////////////////////////////////////////////////////////////////

static void client_main(void * arg )
{
  (void) arg;

  static corpc_service resource_service = {
    .name = "get_resource",
    .methods = {
      { .name = "get_resource", .proc = resource_service_get_resource},
      { .name = NULL },
    }
  };

  static const corpc_service * my_services[] = {
    &resource_service,
    NULL
  };


  CF_DEBUG("Started");


  g_smaster_channel = corpc_channel_open(&(struct corpc_channel_open_args ) {
        .connect_address = "127.0.0.1",
        .connect_port = 6008,

        .ssl_ctx = NULL,

        .services = my_services,

        .onstatechanged = on_channel_state_changed,

        .keep_alive = {
          .enable = true,
          .keepidle = 5,
          .keepintvl = 3,
          .keepcnt = 5
        }
      });

  if ( !g_smaster_channel ) {
    CF_FATAL("corpc_channel_open() fails");
    goto end;
  }

  if ( !authenticate(g_smaster_channel, "iam", "password") ) {
    CF_FATAL("authenticate() fails");
    goto end;
  }


  if ( !request_smaster_resources(g_smaster_channel) ) {
    CF_FATAL("request_smaster_resources() fails");
    goto end;
  }

//  if ( !read_mails(g_smaster_channel) ) {
//    CF_FATAL("read_mails() fails");
//    goto end;
//  }


end:


  CF_DEBUG("C corpc_channel_close()");
  corpc_channel_close(&g_smaster_channel);

  CF_DEBUG("Finished");
}


//////////////////

int main(int argc, char *argv[])
{



  for ( int i = 1; i < argc; ++i ) {

    if ( strcmp(argv[i], "help") == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 ) {
      printf("Usage:\n");
      printf(" client --server <addrs:port> --cid <client-id> --rid <resource-id>"
          "\n");
      return 0;
    }


    /////////////////////////////////////////////////////////////////////////////////////////
    if ( strcmp(argv[i], "--server") == 0 ) {

      if ( ++i >= argc ) {
        fprintf(stderr, "Missing argument after %s command line switch\n", argv[i - 1]);
        return 1;
      }

      if ( sscanf(argv[i], "%255[^:]:%hu", g_server_address, &g_server_port) < 1 ) {
        fprintf(stderr, "Can not parse server address '%s'\n", argv[i]);
        return 1;
      }
    }


    /////////////////////////////////////////////////////////////////////////////////////////
    else if ( strcmp(argv[i], "--cid") == 0 ) {

      if ( ++i >= argc ) {
        fprintf(stderr, "Missing argument after %s command line switch\n", argv[i - 1]);
        return 1;
      }

      strncpy(g_client_id, argv[i], sizeof(g_client_id)-1);
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    else if ( strcmp(argv[i], "--rid") == 0 ) {

      if ( ++i >= argc ) {
        fprintf(stderr, "Missing argument after %s command line switch\n", argv[i - 1]);
        return 1;
      }

      strncpy(g_resource_id, argv[i], sizeof(g_client_id)-1);
    }

    /////////////////////////////////////////////////////////////////////////////////////////
    else if ( strcmp(argv[i], "--n") == 0 ) {
      g_q = false;
    }



    /////////////////////////////////////////////////////////////////////////////////////////
    else {
      fprintf(stderr, "Invalid argument %s\n", argv[i]);
      return 1;
    }
  }






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
