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
#include <cuttle/ssl/x509.h>
#include <cuttle/corpc/corpc-server.h>

#include "../proto/smaster.h"

typedef
struct client_context {
  corpc_channel * channel; // save for event notifications
} client_context;


static bool event_sender_finished = false;
static bool event_receiver_finished = false;


static char CAcert[PATH_MAX];
static char ServerCert[PATH_MAX];
static char ServerKey[PATH_MAX];
static SSL_CTX * g_ssl_ctx;


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

  while ( i++ < 10 ) {

    CF_DEBUG("C corpc_stream_write_sm_timer_event");
    if ( !corpc_stream_write_sm_timer_event(st, &event) ) {
      CF_CRITICAL("corpc_stream_write_sm_timer_event() fails: %s", strerror(errno));
      break;
    }
    CF_DEBUG("R corpc_stream_write_sm_timer_event");

    co_sleep(750);
  }

  CF_DEBUG("FINISHED LOOP");


end:

  corpc_close_stream(&st);

  cleanup_sm_timer_event(&event);
  event_sender_finished = true;
  CF_DEBUG("FINISHED");
}


static void cf_openssl_free_string(char ** buf) {
  if ( buf && *buf ) {
    OPENSSL_free(*buf);
    *buf = NULL;
  }
}

///////////////////////////////////////////////////////////////////////////////////////////
static bool on_accept_client_connection(const corpc_channel * channel)
{
  const SSL * ssl = NULL;
  X509 * cert = NULL;
  X509_NAME * subj = NULL;
  X509_NAME * issuer = NULL;
  char * subjstring = NULL;
  char * subjName = NULL;
  char * text = NULL;

  CF_NOTICE("ACCEPTED NEW CLIENT CONNECTION");

  if ( !(ssl = corpc_channel_get_ssl(channel)) ) {
    CF_NOTICE("CONNECTION IS NOT SECURED: SSL=NULL");
    goto end;
  }

  if ( !(cert = SSL_get_peer_certificate(ssl)) ) {
    CF_NOTICE("Client DOES NOT PROVIDE X509 cerificate");
    goto end;
  }

  CF_NOTICE("cert serial = %ld", cf_x509_get_serial(cert));

  if ( !(subj = X509_get_subject_name(cert))) {
    CF_NOTICE("X509_get_subject_name() returns NULL");
  }
  else {

    CF_NOTICE("subj:name = %s", (text = cf_x509_get_text_entry(subj, NID_name)));
    cf_openssl_free_string(&text);

    CF_NOTICE("subj:commonName = %s", (text = cf_x509_get_text_entry(subj, NID_commonName)));
    cf_openssl_free_string(&text);

    CF_NOTICE("subj:localityName = %s", (text = cf_x509_get_text_entry(subj, NID_localityName)));
    cf_openssl_free_string(&text);

    CF_NOTICE("subj:Domain = %s", (text = cf_x509_get_text_entry(subj, NID_Domain)));
    cf_openssl_free_string(&text);
  }


  if ( !(issuer = X509_get_issuer_name(cert)) ) {
    CF_NOTICE("NO CERT ISSUER");
  }
  else {

    CF_NOTICE("issuer:name = %s", (text = cf_x509_get_text_entry(issuer, NID_name)));
    cf_openssl_free_string(&text);

    CF_NOTICE("issuer:commonName = %s", (text = cf_x509_get_text_entry(issuer, NID_commonName)));
    cf_openssl_free_string(&text);

    CF_NOTICE("issuer:localityName = %s", (text = cf_x509_get_text_entry(issuer, NID_localityName)));
    cf_openssl_free_string(&text);

    CF_NOTICE("issuer:Domain = %s", (text = cf_x509_get_text_entry(issuer, NID_Domain)));
    cf_openssl_free_string(&text);
  }

end:

  OPENSSL_free(subjName);
  OPENSSL_free(subjstring);
  X509_free(cert);

  return true; // keep connection active anyway
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


int main(int argc, char *argv[])
{
  corpc_server * server = NULL;
  bool fok = false;


  for ( int i = 1; i < argc; ++i ) {

    if ( strcmp(argv[i], "help") == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 ) {
      printf("Usage:\n");
      printf(" server "
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

  CF_DEBUG("C cf_ssl_initialize()");

  if ( !cf_ssl_initialize() ) {
    CF_FATAL("cf_ssl_initialize() fails");
    goto end;
  }


  if ( *CAcert || *ServerCert || *ServerKey ) {

    g_ssl_ctx = cf_ssl_create_context(&(struct cf_ssl_create_context_args ) {
          .enabled_ciphers = "ALL",
          .ssl_verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,

          .pem_root_certs = (const char *[] ) { CAcert },
          .nb_pem_root_certs = 1,

          .keycert_file_pairs = (struct cf_keycert_pem_file_pair[] ) {
                { .cert = ServerCert, .key = ServerKey } },
          .nb_keycert_file_pairs = 1,
        });

    if ( !g_ssl_ctx ) {
      CF_FATAL("cf_ssl_create_context() fails");
      goto end;
    }
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

          .ssl_ctx = g_ssl_ctx,

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
