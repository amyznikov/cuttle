/*
 * ssl-context.h
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef ___cuttle_ssl_context_h___
#define ___cuttle_ssl_context_h___

#include <openssl/conf.h>
#include <openssl/ssl.h>
#include <cuttle/ssl/error.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cf_ssl_create_context_args {

  const char * enabled_ciphers;

  const SSL_METHOD * meth; // defaults to TLSv1_2_method()

  int ssl_opts; // defaults to SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION

  int ssl_verify_mode; // defaults to SSL_VERIFY_NONE
  int ssl_verify_depth; // defaults to 0
  int (*ssl_verify_cb) (int, X509_STORE_CTX *);

  const char ** pem_root_certs; // CA certificates
  int nb_pem_root_certs;

  struct cf_keycert_pem_file_pair {
    const char * cert;
    const char * key;
  } const * keycert_file_pairs;

  int nb_keycert_file_pairs;


  // Todo:
  //
  // https://wiki.openssl.org/index.php/Hostname_validation
  //
  // $ man SSL_CTX_load_verify_locations
  //
  //  const char ** pem_root_cert_locations;
  //  int nb_pem_root_cert_locations;
  //
  //  client_CA_list;
};

SSL_CTX * cf_ssl_create_context(const struct cf_ssl_create_context_args * args);
void cf_ssl_delete_context(SSL_CTX ** ssl_ctx);



#ifdef __cplusplus
}
#endif

#endif /* ___cuttle_ssl_context_h___ */
