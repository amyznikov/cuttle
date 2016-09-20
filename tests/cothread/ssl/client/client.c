/*
 * client.c
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <netinet/in.h>

#include <cuttle/debug.h>
#include <cuttle/sockopt.h>
#include <cuttle/ssl/init-ssl.h>
#include <cuttle/cothread/ssl.h>




#define CLIENT_THREAD_STACK_SIZE  (1024*1024)


static char CAcert[PATH_MAX];
static char ClientCert[PATH_MAX];
static char ClientKey[PATH_MAX];
static SSL_CTX * g_ssl_ctx;

static bool finished;


static void client_thread(void * arg)
{
  char buf[1024] = "It's my live!";
  ssize_t cbrecv, cbsent;
  co_ssl_socket * ssl_sock = NULL;

  (void)(arg);

  CF_DEBUG("Started");


  CF_DEBUG("C co_ssl_server_connect()");

  ssl_sock = co_ssl_server_connect_new("localhost", 6008,
      &(const co_ssl_connect_opts) {
      .ssl_ctx = g_ssl_ctx,
      .sock_type = SOCK_STREAM,
      .proto = IPPROTO_TCP,
      .tmout = 5000
      });

  if ( !ssl_sock ) {
    CF_CRITICAL("co_ssl_server_connect() fails");
    goto end;
  }
  CF_DEBUG("R co_ssl_server_connect()");

  CF_DEBUG("C co_ssl_socket_send()");
  if ( (cbsent = co_ssl_socket_send(ssl_sock, buf, strlen(buf) + 1)) <= 0 ) {
    CF_CRITICAL("co_ssl_socket_send() fails: %s", strerror(errno));
    goto end;
  }
  CF_DEBUG("R co_ssl_socket_send()");

  CF_DEBUG("C co_ssl_socket_recv()");
  if ( (cbrecv = co_ssl_socket_recv(ssl_sock, buf, sizeof(buf) - 1)) <= 0 ) {
    CF_CRITICAL("co_ssl_socket_recv() fails");
    goto end;
  }
  CF_DEBUG("R co_ssl_socket_recv()");

  CF_DEBUG("%zd bytes received: %s", cbrecv, buf);

end:

  CF_DEBUG("C co_ssl_socket_close(&ssl_sock, false)");
  co_ssl_socket_destroy(&ssl_sock, false);
  CF_DEBUG("R co_ssl_socket_close(&ssl_sock, false)");

  CF_DEBUG("Finished");
  finished = true;
}

int main(int argc, char *argv[])
{
  for ( int i = 1; i < argc; ++i ) {

    if ( strcmp(argv[i], "help") == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 ) {
      printf("Usage:\n");
      printf(" client "
          "-CA <CAcert> "
          "-Cert <ClientCert> "
          "-Key <ClientKey>"
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
        fprintf(stderr, "Missing ClientCert\n");
        return 1;
      }
      strncpy(ClientCert, argv[i], sizeof(ClientCert) - 1);
    }
    else if ( strcmp(argv[i], "-Key") == 0 ) {
      if ( ++i == argc ) {
        fprintf(stderr, "Missing ClientKey\n");
        return 1;
      }
      strncpy(ClientKey, argv[i], sizeof(ClientKey) - 1);
    }
    else {
      fprintf(stderr, "Invalid argument %s\n", argv[i]);
      return 1;
    }
  }


  cf_set_logfilename("stderr");
  cf_set_loglevel(CF_LOG_DEBUG);

  if ( !cf_ssl_initialize() ) {
    CF_FATAL("cf_ssl_initialize() fails");
    goto end;
  }

  g_ssl_ctx = cf_ssl_create_context( &(struct cf_ssl_create_context_args ) {
        .enabled_ciphers = "ALL",
        .ssl_verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,

        .pem_root_certs = (const char *[] ) { CAcert },
        .nb_pem_root_certs = 1,

        .keycert_file_pairs = (struct cf_keycert_pem_file_pair[]) {
            { .cert = ClientCert, .key = ClientKey } },
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

  if ( !co_schedule(client_thread, NULL, CLIENT_THREAD_STACK_SIZE )) {
    CF_FATAL("co_schedule(server_thread) fails: %s", strerror(errno));
    goto end;
  }

  while ( !finished ) {
    sleep(1);
  }

end:

  return 0;
}













