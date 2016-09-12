/*
 * ssl-error.c
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <cuttle/debug.h>
#include <cuttle/ssl/error.h>


#define CF_LIB_NAME        "libCUTTLE"

static int cf_lib_id = 0;

#define ERR_FUNC(func)      ERR_PACK(0,func,0)
#define ERR_REASON(reason)  ERR_PACK(0,0,reason)


// for f in $(egrep -o 'cf_([^(]*)'  ssl-pkey.h ); do echo "\"$f\","; done
static const char * cf_crypto_functions[] = {
    // ssl-x509.h
    "cf_x509_new",

    // ssl-pkey.h
    "cf_pkey_new",
    "cf_pkey_free",
    "cf_write_pem_public_key",
    "cf_read_pem_public_key",
    "cf_write_pem_private_key",
    "cf_read_pem_private_key",
    "cf_write_pem_private_key_enc",
    "cf_read_pem_private_key_enc",
    "cf_write_pem_public_key_str",
    "cf_read_pem_public_key_str",
    "cf_write_pem_private_key_str",
    "cf_read_pem_private_key_str",
    "cf_write_public_key_bits",
    "cf_read_public_key_bits",
    "cf_write_private_key_bits",
    "cf_read_private_key_bits",
    "cf_write_public_key_hex_str",
    "cf_read_public_key_hex_str",
    "cf_write_private_key_hex_str",
    "cf_read_private_key_hex_str",
    "cf_write_public_key_hex_fp",
    "cf_read_public_key_hex_fp",
    "cf_write_private_key_hex_fp",
    "cf_read_private_key_hex_fp",
    "cf_write_public_key_hex",
    "cf_read_public_key_hex",
    "cf_write_private_key_hex",
    "cf_read_private_key_hex",

};

static ERR_STRING_DATA cf_crypto_str_reasons[]= {
  {ERR_REASON(CF_SSL_ERR_OPENSSL)     , "OpenSSL error"},
  {ERR_REASON(CF_SSL_ERR_INVALID_ARG) , "Invalid argument"},
  {ERR_REASON(CF_SSL_ERR_STDIO)       , "stdio error"},
  {ERR_REASON(CF_SSL_ERR_MALLOC)      , "malloc() fails" },
  {ERR_REASON(CF_SSL_ERR_PTHREAD)     , "pthread error" },
  {ERR_REASON(CF_SSL_ERR_EPOLL)       , "epoll error" },
  {ERR_REASON(CF_SSL_ERR_CUTTLE)      , "libcuttle error" },
  {ERR_REASON(CF_SSL_ERR_APP)         , "app error" },

  {0,NULL}
};

#ifdef CF_LIB_NAME
static ERR_STRING_DATA cf_lib_name[] = {
  { 0, CF_LIB_NAME },
  { 0, NULL }
};
#endif


static int sscmp(const void * p1, const void * p2)
{
  return strcmp(*(const char **) p1, *(const char **) p2);
}


void cf_init_ssl_error_strings(void)
{
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  static ERR_STRING_DATA * err_string_data = NULL;

  pthread_mutex_lock(&mtx);

  if ( !err_string_data ) {

    size_t i, n;

    cf_lib_id = ERR_get_next_error_library();
    ERR_load_strings(cf_lib_id, cf_crypto_str_reasons);

    n = sizeof(cf_crypto_functions) / sizeof(cf_crypto_functions[0]);

    if ( (err_string_data = calloc(n + 1, sizeof(ERR_STRING_DATA))) ) {

      qsort(cf_crypto_functions, n, sizeof(cf_crypto_functions[0]), sscmp);

      for ( i = 0; i < n; ++i ) {
        err_string_data[i].error = ERR_FUNC(i + 100);
        err_string_data[i].string = cf_crypto_functions[i];
      }

      ERR_load_strings(cf_lib_id, err_string_data);
    }


#ifdef CF_LIB_NAME
    cf_lib_name->error = ERR_PACK(cf_lib_id, 0, 0);
    ERR_load_strings(0, cf_lib_name);
#endif

  }

  pthread_mutex_unlock(&mtx);
}


void cf_ssl_error(const char * func, int reason, char * file, int line, char * format, ...)
{
  int function = 0;

  cf_init_ssl_error_strings();

  if ( func != NULL ) {
    const char ** item;
    const size_t n = sizeof(cf_crypto_functions) / sizeof(cf_crypto_functions[0]);
    if ( (item = bsearch(&func, cf_crypto_functions, n, sizeof(cf_crypto_functions[0]), sscmp)) ) {
      function = (int) ((item - cf_crypto_functions) + 100);
    }
  }

  ERR_PUT_error(cf_lib_id, function, reason, file, line);

  if ( format && *format ) {

    va_list arglist;
    char * errmsg = NULL;

    va_start(arglist, format);
    if ( vasprintf(&errmsg, format, arglist) >= 0 ) {
      ERR_add_error_data(1, errmsg);
    }
    va_end(arglist);

    free(errmsg);
  }
}


const char * cf_get_ssl_error_string(SSL * ssl, int call_status)
{
  int ssl_status;

  switch ( ssl_status = SSL_get_error(ssl, call_status) ) {
    case SSL_ERROR_NONE :
      return "SSL_ERROR_NONE";
    case SSL_ERROR_WANT_CONNECT :
      return "SSL_ERROR_WANT_CONNECT";
    case SSL_ERROR_WANT_ACCEPT :
      return "SSL_ERROR_WANT_ACCEPT";
    case SSL_ERROR_WANT_WRITE :
      return "SSL_ERROR_WANT_WRITE";
    case SSL_ERROR_WANT_READ :
      return "SSL_ERROR_WANT_READ";
    case SSL_ERROR_WANT_X509_LOOKUP :
      return "SSL_ERROR_WANT_X509_LOOKUP";
    case SSL_ERROR_ZERO_RETURN :
      return "Shutdown";
    case SSL_ERROR_SYSCALL :
      return strerror(errno);
  }
  return "";
}
