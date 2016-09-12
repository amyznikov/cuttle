/*
 * ssl-context.c
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 *
 *  https://wiki.openssl.org/index.php/SSL_and_TLS_Protocols
 *  https://wiki.openssl.org/index.php/SSL/TLS_Client
 *  https://wiki.openssl.org/index.php/Simple_TLS_Server
 *
 */


#include <cuttle/ssl/ssl-context.h>
#include <cuttle/ssl/error.h>
#include <openssl/ecdh.h>
#include <stdbool.h>


#ifndef SSL_CTRL_SET_ECDH_AUTO
/**
 * See https://wiki.openssl.org/index.php/Diffie-Hellman_parameters
 */
static bool SSL_CTX_set_ecdh_auto(SSL_CTX * ctx, bool onoff)
{
  (void)(onoff);

  EC_KEY * ecdh = NULL;
  bool fok = false;

  if ( !(ecdh = EC_KEY_new_by_curve_name (NID_X9_62_prime256v1))) {
    goto end;
  }

  if ( SSL_CTX_set_tmp_ecdh (ctx, ecdh) != 1 ) {
    goto end;
  }

  fok= true;

end:

  if ( ecdh ) {
    EC_KEY_free (ecdh);
  }

  return fok;
}

#endif


SSL_CTX * cf_ssl_create_context(const struct cf_ssl_create_context_args * args)
{
  const SSL_METHOD * meth;
  int ssl_opts;

  SSL_CTX * ssl_ctx = NULL;
  bool fok = false;


  if ( !(meth = args->meth) ) {
    meth = TLSv1_2_method();
  }

  if ( !(ssl_opts = args->ssl_opts ) ) {
    ssl_opts = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION;
  }

  if ( !(ssl_ctx = SSL_CTX_new(meth)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_CTX_new() fails");
    goto end;
  }

  if ( ssl_opts ) {
    SSL_CTX_set_options(ssl_ctx, ssl_opts);
  }

  if ( !SSL_CTX_set_ecdh_auto(ssl_ctx, true) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_CTX_set_ecdh_auto() fails");
    goto end;
  }

  if ( args->enabled_ciphers && *args->enabled_ciphers ) {
    if ( SSL_CTX_set_cipher_list(ssl_ctx, args->enabled_ciphers) != 1 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_CTX_set_cipher_list(%s) fails", args->enabled_ciphers);
      goto end;
    }
  }


  if ( args->ssl_verify_mode || args->ssl_verify_cb ) {
    SSL_CTX_set_verify(ssl_ctx, args->ssl_verify_mode, args->ssl_verify_cb);
  }

  if ( args->ssl_verify_depth > 0 ) {
    SSL_CTX_set_verify_depth(ssl_ctx, args->ssl_verify_depth);
  }

  for ( int i = 0; i < args->nb_pem_root_certs; ++i ) {
    const char * certfile = args->pem_root_certs[i];
    if ( SSL_CTX_load_verify_locations(ssl_ctx, certfile, NULL) != 1 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_CTX_load_verify_locations(%s) fails", certfile);
      goto end;
    }
  }


  for ( int i = 0; i < args->nb_keycert_file_pairs; ++i ) {

    const char * certfile = args->keycert_file_pairs[i].cert;
    const char * keyfile = args->keycert_file_pairs[i].key;

    if ( !certfile || !*certfile ) {
      CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Empty cert file name in keycert_file_pairs[%d]", i);
      goto end;
    }

    if ( !keyfile || !*keyfile ) {
      CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Empty key file name in keycert_file_pairs[%d]", i);
      goto end;
    }

    if ( SSL_CTX_use_certificate_file(ssl_ctx, certfile, SSL_FILETYPE_PEM) != 1 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_CTX_use_certificate_file(%s) fails", certfile);
      goto end;
    }

    if ( SSL_CTX_use_PrivateKey_file(ssl_ctx, keyfile, SSL_FILETYPE_PEM) != 1 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_CTX_use_PrivateKey_file(%s) fails", keyfile);
      goto end;
    }

    if ( SSL_CTX_check_private_key(ssl_ctx) != 1 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "SSL_CTX_check_private_key(%s,%s) fails", certfile, keyfile);
      goto end;
    }
  }


  fok = true;

end: ;

  if ( !fok && ssl_ctx ) {
    SSL_CTX_free(ssl_ctx);
    ssl_ctx = NULL;
  }

  return ssl_ctx;

}

void cf_ssl_delete_context(SSL_CTX ** ssl_ctx)
{
  if ( ssl_ctx && *ssl_ctx ) {
    SSL_CTX_free(*ssl_ctx);
    *ssl_ctx = NULL;
  }
}
