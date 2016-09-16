/*
 * server.c
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>

#include <cuttle/debug.h>
#include <cuttle/sockopt.h>
#include <cuttle/ssl/init-ssl.h>
#include <cuttle/cothread/ssl-server.h>




#define SERVER_THREAD_STACK_SIZE (512*1024)
#define PROCESSOR_THREAD_STACK_SIZE (1024*1024)

static char CAcert[PATH_MAX];
static char ServerCert[PATH_MAX];
static char ServerKey[PATH_MAX];
static SSL_CTX * g_ssl_ctx;


struct connection_context {
  co_ssl_listening_port * sslp;
  co_ssl_socket * ssl_sock;
};


static void process_connection(void * arg)
{
  struct connection_context * context = arg;

  char buf[1024] = "";
  ssize_t cbrecv, cbsent;

  co_ssl_socket * ssl_sock = context->ssl_sock;

  CF_DEBUG("Started");

  if ( !co_ssl_socket_accept(ssl_sock) ) {
    CF_CRITICAL("co_ssl_socket_accept() fails");
    goto end;
  }

  if ( (cbrecv = co_ssl_socket_recv(ssl_sock, buf, sizeof(buf) - 1)) < 0 ) {
    CF_CRITICAL("co_ssl_socket_recv() fails");
    goto end;
  }

  CF_DEBUG("%zd bytes received: %s", cbrecv, buf);

  strfry(buf);
  CF_DEBUG("Sending back strfry-ed message:'%s'", buf);

//  co_sleep(1000);

  if ( (cbsent = co_ssl_socket_send(ssl_sock, buf, strlen(buf) + 1)) < 0 ) {
    CF_CRITICAL("co_ssl_socket_send() fails");
    goto end;
  }

  CF_DEBUG("%zd bytes sent: %s", cbsent, buf);

end:

  co_ssl_socket_close(&ssl_sock, false);

  CF_DEBUG("Finished");
}


static bool on_accept_connection(struct co_ssl_listening_port * sslp, co_ssl_socket * ssl_sock)
{
  struct connection_context * context;

  bool fok = false;

  if ( !(context = malloc(sizeof(*context))) ) {
    CF_CRITICAL("malloc(connection_context) fails");
    goto end;
  }

  context->ssl_sock = ssl_sock;
  context->sslp = sslp;

  if ( !co_schedule(process_connection, context, PROCESSOR_THREAD_STACK_SIZE) ) {
    CF_CRITICAL("co_schedule(process_connection) fails");
    goto end;
  }

end:

  if ( !fok ) {
    free(context);
  }

  return fok;
}



int main(int argc, char *argv[])
{
  co_ssl_server * server = NULL;


  for ( int i = 1; i < argc; ++i ) {

    if ( strcmp(argv[i], "help") == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 ) {
      printf("Usage:\n");
      printf(" client "
          "-CA <CAcert> "
          "-Cert <ServerCert> "
          "-Key <ServerKey>"
          "\n");
      return 0;
    }

    if ( strcmp(argv[i], "-CA") == 0 ) {
      if ( ++i == argc ) {
        fprintf(stderr, "Missing CAcert\n");
        return 1;
      }
      strncpy(CAcert, argv[i], sizeof(CAcert) - 1);
    }
    else if ( strcmp(argv[i], "-Cert") == 0 ) {
      if ( ++i == argc ) {
        fprintf(stderr, "Missing ServerCert\n");
        return 1;
      }
      strncpy(ServerCert, argv[i], sizeof(ServerCert) - 1);
    }
    else if ( strcmp(argv[i], "-Key") == 0 ) {
      if ( ++i == argc ) {
        fprintf(stderr, "Missing ServerKey\n");
        return 1;
      }
      strncpy(ServerKey, argv[i], sizeof(ServerKey) - 1);
    }
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


  g_ssl_ctx = cf_ssl_create_context( &(struct cf_ssl_create_context_args ) {
        .enabled_ciphers = "ALL",
        .ssl_verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,

        .pem_root_certs = (const char *[] ) { CAcert },
        .nb_pem_root_certs = 1,

        .keycert_file_pairs = (struct cf_keycert_pem_file_pair[]) {
            { .cert = ServerCert, .key = ServerKey } },
        .nb_keycert_file_pairs = 1,
      });

  if ( !g_ssl_ctx ) {
    CF_FATAL("cf_ssl_create_context() fails");
    goto end;
  }


  if ( !co_scheduler_init(2) ) {
    CF_FATAL("co_scheduler_init() fails: %s", strerror(errno));
    goto end;
  }



  if ( !(server = co_ssl_server_new(NULL)) ) {
    CF_FATAL("co_server_new() fails");
    goto end;
  }

  co_ssl_server_add_new_port(server, &(struct co_ssl_listening_port_opts ) {
        .bind_address.in = {
          .sin_family = AF_INET,
          .sin_port = htons(6008),
          .sin_addr.s_addr = 0,
        },
        .ssl_ctx = g_ssl_ctx,
        .onaccept = on_accept_connection,
        .accepted_stack_size = PROCESSOR_THREAD_STACK_SIZE
      });


  if ( !co_ssl_server_start(server) ) {
    CF_FATAL("co_server_start() fails");
    goto end;
  }


  while ( 42 ) {
    sleep(1);
  }

end:

  return 0;
}
