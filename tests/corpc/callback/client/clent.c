/*
 * clent.c
 *
 *  Created on: Sep 10, 2016
 *      Author: amyznikov
 */

#include <stdio.h>
#include <unistd.h>
#include <cuttle/debug.h>
#include <cuttle/cothread/scheduler.h>
#include <cuttle/ssl/init-ssl.h>
#include <cuttle/ssl/x509.h>

#include "../proto/smaster.h"



static bool event_sender_finished = false;
static bool event_receiver_running = false;
static bool event_receiver_finished = false;
static bool client_main_finished = false;

static co_thread_lock_t thread_lock = CO_THREAD_LOCK_INITIALIZER;

/////////////////////////////////////////////////////////////////////////////////////////////

static void set_event(bool * event)
{
  co_thread_lock(&thread_lock);
  *event = true;
  co_thread_signal(&thread_lock);
  co_thread_unlock(&thread_lock);
}

static void wait_event(bool * event)
{
  co_thread_lock(&thread_lock);
  while ( !*event ) {
    co_thread_wait(&thread_lock, -1);
  }
  co_thread_unlock(&thread_lock);
}


/////////////////////////////////////////////////////////////////////////////////////////////

static void send_timer_events_to_server(void * arg)
{
  corpc_channel * channel = arg;
  corpc_stream * st = NULL;
  sm_timer_event event;
  int i = 0;

  CF_DEBUG("STARTED");

  init_sm_timer_event(&event, "CLIENT-TIMER-EVENT");

  st = corpc_open_stream(channel,
      &(struct corpc_open_stream_opts ) {
            .service = k_smaster_events_service_name,
            .method = k_smaster_events_ontimer_methd_name,
          });

  if ( !st ) {
    CF_CRITICAL("corpc_open_stream() fails");
    goto end;
  }

  while ( i++ < 10 && corpc_stream_write_sm_timer_event(st, &event) ) {
    co_sleep(500);
  }

end:

  corpc_close_stream(&st);

  cleanup_sm_timer_event(&event);

  set_event(&event_sender_finished);

  CF_DEBUG("FINISHED");
}

/////////////////////////////////////////////////////////////////////////////////////////////

static void receive_timer_events_from_server(corpc_stream * st)
{
  sm_timer_event e;
  CF_DEBUG("ENTER");

  set_event(&event_receiver_running);


  init_sm_timer_event(&e, NULL);

  while ( corpc_stream_read_sm_timer_event(st, &e))  {
    CF_DEBUG("%s", e.msg);
    cleanup_sm_timer_event(&e);
  }

  CF_DEBUG("corpc_stream_read_sm_timer_event() fails");

  cleanup_sm_timer_event(&e);

  set_event(&event_receiver_finished);

  CF_DEBUG("LEAVE");
}

static const corpc_service server_event_listener_service = {
  .name = k_smaster_events_service_name,
  .methods = {
    { .name = k_smaster_events_ontimer_methd_name, .proc = receive_timer_events_from_server },
    { .name = NULL },
  }
};

static const corpc_service * client_services[] = {
  &server_event_listener_service,
  NULL
};


/////////////////////////////////////////////////////////////////////////////////////////////


static char CAcert[PATH_MAX];
static char ClientCert[PATH_MAX];
static char ClientKey[PATH_MAX];
static SSL_CTX * g_ssl_ctx;


static void cf_openssl_free_string(char ** buf) {
  if ( buf && *buf ) {
    OPENSSL_free(*buf);
    *buf = NULL;
  }
}

static bool onconnect(const corpc_channel * channel)
{
  const SSL * ssl = NULL;
  X509 * cert = NULL;
  X509_NAME * subj = NULL;
  char * entry = NULL;
  long serial;

  if ( !(ssl = corpc_channel_get_ssl(channel)) ) {
    CF_NOTICE("No ssl");
    goto end;
  }

  if ( !(cert = SSL_get_peer_certificate(ssl)) ) {
    CF_NOTICE("NO PEER CERTIFICATE");
    goto end;
  }

  serial = cf_x509_get_serial(cert);
  CF_NOTICE("serial: %ld", serial);

  if ( !(subj = X509_get_subject_name(cert)) ) {
    CF_CRITICAL("X509_get_subject_name() fails");
  }
  else {

    CF_NOTICE("subj: name=%s", (entry = cf_x509_get_name(subj)));
    cf_openssl_free_string(&entry);

    CF_NOTICE("subj: commonName=%s", (entry = cf_x509_get_common_name(subj)));
    cf_openssl_free_string(&entry);

    CF_NOTICE("subj: country=%s", (entry = cf_x509_get_country(subj)));
    cf_openssl_free_string(&entry);
  }

end:

  X509_free(cert);

  return true;
}

static void client_main(void * arg )
{
  (void) arg;

  corpc_channel * channel;

  CF_DEBUG("Started");


  channel = corpc_channel_new(&(struct corpc_channel_opts ) {
        .connect_address = "localhost",
        .connect_port = 6008,
        .services = client_services,
        .ssl_ctx = g_ssl_ctx,
        .onconnect = onconnect,
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


  if ( !co_schedule(send_timer_events_to_server, channel, 1024 * 1024) ) {
    CF_FATAL("co_schedule(start_timer_events) fails: %s", strerror(errno));
    goto end;
  }

  CF_DEBUG("C wait_event(&event_sender_finished)");
  wait_event(&event_sender_finished);
  CF_DEBUG("R wait_event(&event_sender_finished)");

  if ( event_receiver_running ) {
    CF_DEBUG("C wait_event(&event_receiver_finished)");
    wait_event(&event_receiver_finished);
    CF_DEBUG("R wait_event(&event_receiver_finished)");
  }

end:


  CF_DEBUG("C corpc_channel_relase()");
  corpc_channel_close(channel);
  CF_DEBUG("R corpc_channel_relase()");

  set_event(&client_main_finished);
  CF_DEBUG("Finished");
}


//////////////////

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
    CF_FATAL("cf_ssl_initialize() fails: %s", strerror(errno));
    goto end;
  }

  if ( *CAcert || *ClientCert || *ClientKey ) {

    g_ssl_ctx = cf_ssl_create_context( &(struct cf_ssl_create_context_args ) {
          .enabled_ciphers = "ALL",
          .ssl_verify_mode = SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,

          .pem_root_certs = (const char *[] ) { CAcert },
          .nb_pem_root_certs = 1,

          .keycert_file_pairs = (struct cf_keycert_pem_file_pair[]) {
              { .cert = ClientCert , .key = ClientKey } },
          .nb_keycert_file_pairs = 1,
        });

    if ( !g_ssl_ctx ) {
      CF_FATAL("cf_ssl_create_context() fails");
      goto end;
    }

  }

  if ( !co_scheduler_init(2) ) {
    CF_FATAL("co_scheduler_init() fails: %s", strerror(errno));
    goto end;
  }

  if ( !co_schedule(client_main, NULL, 1024 * 1024) ) {
    CF_FATAL("co_schedule(server_thread) fails: %s", strerror(errno));
    goto end;
  }

  while ( !client_main_finished ) {
    usleep(100 * 1000);
  }

end:

  return 0;
}
