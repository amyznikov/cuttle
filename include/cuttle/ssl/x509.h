/*
 * cuttle/ssl/ssl-x509.h
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 */

// #pragma once

#ifndef __cuttle_ssl_x509_h__
#define __cuttle_ssl_x509_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509v3.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef
struct cf_x509_ext {
  int nid;
  const char * value;
} cf_x509_ext;

typedef
struct cf_x509_create_args {

  const EVP_MD * md;

  struct {
    X509 * cert;
    EVP_PKEY * pkey;
  } ca;

  struct {
    const char * keytype;
    const char * params;
    EVP_PKEY * pubkey;
  } keygen;

  struct {
    const char * country;
    const char * state;
    const char * city;
    const char * company;
    const char * department;
    const char * common_name;
    const char * email;
  } subj;

  struct {
    struct {
      time_t time;
    } notBefore;

    struct {
      time_t time;
      struct {
        long days;
        long hours;
        long minutes;
        long seconds;
      } period;
    } notAfter;
  } valididy;

  const struct cf_x509_ext * ext;
  int nbext;

  int serial;

} cf_x509_create_args;


X509 * cf_x509_new(EVP_PKEY ** ppkey, const cf_x509_create_args * args);
void cf_x509_free(X509 ** x);

bool cf_write_pem_x509(X509 * x, const char * fname);
X509 * cf_read_pem_x509(const char * fname);

bool cf_write_pem_x509_fp(X509 * x, FILE * fp);
X509 * cf_read_pem_x509_fp(FILE * fp);

char * cf_write_pem_x509_str(X509 * x);
X509 * cf_read_pem_x509_str(const char * s);


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_x509_h__ */
