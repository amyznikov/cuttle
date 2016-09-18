/*
 * x509.c
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 *
 *
 *  See openssl/demos/x509/mkcert.c
 */


#include <cuttle/ssl/error.h>
#include <cuttle/ssl/pkey.h>
#include <cuttle/ssl/x509.h>
#include "getfp.h"
#include <string.h>
#include <strings.h>


//static bool cf_x509_add_txt_entry(X509_NAME * name, const char * field, const char * value)
//{
//  return value ? X509_NAME_add_entry_by_txt(name, field, MBSTRING_UTF8, (const uint8_t*) value, -1, -1, 0) : true;
//}


// SN_subject_alt_name
static bool cf_x509_add_ext(X509 * x, X509 * issuer, X509 * subj,  int nid, const char * value)
{
  X509_EXTENSION * ex = NULL;
  X509V3_CTX ctx;
  bool fok = false;

  /* This sets the 'context' of the extensions. */
  /* No configuration database */
  X509V3_set_ctx_nodb(&ctx);

  X509V3_set_ctx(&ctx, issuer, subj, NULL, NULL, 0);

  if ( !(ex = X509V3_EXT_conf_nid(NULL, &ctx, nid, (char * )value)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509V3_EXT_conf_nid() fails: nid=%d value=%s", nid, value);
    goto end;
  }

  if ( !X509_add_ext(x, ex, -1) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_add_ext() fails: nid=%d value=%s", nid, value);
    goto end;
  }

  fok = true;

end:

  if ( ex ) {
    X509_EXTENSION_free(ex);
  }
  return fok;
}


X509 * cf_x509_new(EVP_PKEY ** ppk, const cf_x509_create_args * args)
{
  X509 * x = NULL;
  X509_NAME * name = NULL, * caname = NULL;
  EVP_PKEY * pkey = NULL, * cakey = NULL;
  const char * keytype = NULL, * keyparams = NULL;
  const EVP_MD * md = NULL;

  bool fok = false;

  if ( !ppk ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "pkey not specified");
    goto end;
  }

  if ( (args->ca.cert && !args->ca.pkey) ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "ca.pkey not specified");
    goto end;
  }

  if ( args->ca.pkey && !args->ca.cert ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "ca.cert not specified");
    goto end;
  }

  if ( !(pkey = *ppk) ) {

    if ( !(keytype = args->keygen.keytype) ) {
      keytype = "rsa";
    }

    if ( !(keyparams = args->keygen.params) ) {
      if ( strcasecmp(keytype,"rsa") == 0 ) {
        keyparams = "rsa_keygen_bits:2048";
      }
      else if ( strcasecmp(keytype, "gost94") == 0 || strcasecmp(keytype, "gost2001") == 0 ) {
        keyparams = "paramset:A";
      }
      else if ( strncasecmp(keytype, "dstu4145", 8) == 0 ) {
        keyparams = "curve:uacurve0";
      }
    }

    if ( !(pkey = cf_pkey_new(keytype, keyparams, args->keygen.pubkey)) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_pkey_new() fails");
      goto end;
    }
  }



  if ( !(x = X509_new()) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_new() fails");
    goto end;
  }

  if ( !X509_set_version(x, 2) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_set_version(3) fails");
    goto end;
  }

  if ( !ASN1_INTEGER_set(X509_get_serialNumber(x), args->serial) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "ASN1_INTEGER_set(serial=%d) fails", args->serial);
    goto end;
  }

  if ( !args->valididy.notBefore.time ) {
    if ( !X509_time_adj(X509_get_notBefore(x), 0, NULL) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_time_adj(notBefore) fails");
      goto end;
    }
  }
  else {
    time_t t = args->valididy.notBefore.time;
    if ( !X509_time_adj(X509_get_notBefore(x), 0, &t) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_gmtime_adj(notBefore) fails");
      goto end;
    }
  }

  if ( args->valididy.notAfter.time ) {
    time_t t = args->valididy.notAfter.time;
    if ( !X509_time_adj(X509_get_notAfter(x), 0, &t) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_gmtime_adj(notAfter) fails");
      goto end;
    }
  }
  else {
    long offset = ((args->valididy.notAfter.period.days * 24 + args->valididy.notAfter.period.hours) * 60
        + args->valididy.notAfter.period.minutes) * 60 + args->valididy.notAfter.period.seconds;
    if ( !X509_time_adj(X509_get_notAfter(x), offset, NULL) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_gmtime_adj(notAfter) fails");
      goto end;
    }
  }

  if ( !X509_set_pubkey(x, pkey) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_set_pubkey() fails");
    goto end;
  }

  if ( !(name = X509_get_subject_name(x)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_get_subject_name() fails");
    goto end;
  }

  if ( args->subj.title && !cf_x509_add_text_entry(name, NID_title, args->subj.title) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_title) fails");
    goto end;
  }

  if ( args->subj.name && !cf_x509_add_text_entry(name, NID_name, args->subj.name) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_name) fails");
    goto end;
  }

  if ( args->subj.common_name && !cf_x509_add_text_entry(name, NID_commonName, args->subj.common_name) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_commonName) fails");
    goto end;
  }

  if ( args->subj.altname && !cf_x509_add_text_entry(name, NID_subject_alt_name, args->subj.altname) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_subject_alt_name) fails");
    goto end;
  }

  if ( args->subj.description && !cf_x509_add_text_entry(name, NID_description, args->subj.domain) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_description) fails");
    goto end;
  }

  if ( args->subj.country && !cf_x509_add_text_entry(name, NID_countryName, args->subj.country) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_countryName) fails");
    goto end;
  }

  if ( args->subj.state && !cf_x509_add_text_entry(name, NID_stateOrProvinceName, args->subj.state) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_stateOrProvinceName) fails");
    goto end;
  }

  if ( args->subj.city && !cf_x509_add_text_entry(name, NID_localityName, args->subj.city) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_localityName) fails");
    goto end;
  }

  if ( args->subj.company && !cf_x509_add_text_entry(name, NID_organizationName, args->subj.company) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_organizationName) fails");
    goto end;
  }

  if ( args->subj.department && !cf_x509_add_text_entry(name, NID_organizationalUnitName, args->subj.department) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_organizationalUnitName) fails");
    goto end;
  }

  if ( args->subj.email && !cf_x509_add_text_entry(name, NID_pkcs9_emailAddress, args->subj.email) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_pkcs9_emailAddress) fails");
    goto end;
  }

  if ( args->subj.domain && !cf_x509_add_text_entry(name, NID_Domain, args->subj.domain) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_text_entry(NID_Domain) fails");
    goto end;
  }





  if ( !args->ca.cert ) {
    /* If self signed set the issuer name to be the same as the subject. */
    if ( !X509_set_issuer_name(x, name) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_set_issuer_name() fails");
      goto end;
    }
  }
  else if ( !(caname = X509_get_subject_name(args->ca.cert) ) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_get_subject_name(ca) fails");
    goto end;
  }
  else if ( !X509_set_issuer_name(x, caname) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_set_issuer_name(caname) fails");
    goto end;
  }
  else if ( !X509_set_subject_name(x, name) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_set_subject_name(name) fails");
    goto end;
  }


//  if ( args->bas ) {
//    if ( !cf_x509_add_ext(x, NULL, NULL, NID_basic_constraints, "critical,CA:TRUE") ) {
//      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_ext(ca_kind) fails");
//      goto end;
//    }
//  }
  //add_ext(x, NID_key_usage, "critical,keyCertSign,cRLSign");

  for ( int i = 0; i < args->nbext; ++i ) {
    if ( !cf_x509_add_ext(x, NULL, NULL, args->ext[i].nid, args->ext[i].value) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_x509_add_ext(nid=%d:%s) fails", args->ext[i].nid, args->ext[i].value);
      goto end;
    }
  }


  if ( !(md = args->md) ) {
    md = cf_pkey_get_default_md(pkey);
  }

  if ( !(cakey = args->ca.pkey) ) {
    cakey = pkey; /* will self-signed */
  }

  if ( !X509_sign(x, cakey, md) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "X509_sign() fails");
    goto end;
  }

  if ( !*ppk ) {
    *ppk = pkey;
  }

  fok = true;

end:

  if ( !fok ) {

    cf_x509_free(&x);

    if ( !*ppk ) {
      cf_pkey_free(&pkey);
    }
  }

  return x;
}


void cf_x509_free(X509 ** x)
{
  if ( x && *x ) {
    X509_free(*x);
    *x = NULL;
  }
}


bool cf_write_pem_x509_fp(X509 * x, FILE * fp)
{
  return PEM_write_X509(fp, x) == 1;
}

X509 * cf_read_pem_x509_fp(FILE * fp)
{
  X509 * x = NULL;
  PEM_read_X509(fp, &x, NULL, NULL);
  return x;
}

bool cf_write_pem_x509(X509 * x, const char * fname)
{
  FILE * fp = NULL;
  bool fok = false;

  if ( (fp = cf_getfp(fname, "w", &fok)) ) {
    fok = cf_write_pem_x509_fp(x, fp);
    cf_closefp(&fp);
  }

  return fok;
}

X509 * cf_read_pem_x509(const char * fname)
{
  FILE * fp = NULL;
  X509 * x = NULL;

  if ( (fp = cf_getfp(fname, "r", NULL)) ) {
    x = cf_read_pem_x509_fp(fp);
    cf_closefp(&fp);
  }

  return x;
}

char * cf_write_pem_x509_str(X509 * x)
{
  char * outbuf = NULL;
  BIO * bio = NULL;
  BUF_MEM * mem = NULL;

  if ( !(bio = BIO_new(BIO_s_mem())) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new(BIO_s_mem()) fails");
    goto end;
  }

  if ( PEM_write_bio_X509(bio, x) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_write_bio_X509() fails");
    goto end;
  }

  BIO_get_mem_ptr(bio, &mem);
  if ( !mem ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_get_mem_ptr() fails");
    goto end;
  }

  if ( !(outbuf = OPENSSL_malloc(mem->length+1)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "OPENSSL_malloc(%zu) fails", mem->length + 1);
    goto end;
  }

  memcpy(outbuf, mem->data, mem->length);
  outbuf[mem->length] = 0;

end:

  if ( bio ) {
    BIO_free(bio);
  }

  return outbuf;
}

X509 * cf_read_pem_x509_str(const char * s)
{
  X509 * x = NULL;
  BIO * bio = NULL;

  bool fOk = false;

  if ( !s || !*s ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "No string provided");
    goto end;
  }

  if ( !(bio = BIO_new_mem_buf(s, -1)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new_mem_buf() fails");
    goto end;
  }

  if ( !PEM_read_bio_X509(bio, &x, NULL, NULL) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_read_bio_X509() fails");
    goto end;
  }

  fOk = true;

end:

  if ( bio ) {
    BIO_free(bio);
  }

  if ( x && !fOk ) {
    cf_x509_free(&x);
  }

  return x;
}


long cf_x509_get_serial(const X509 * x)
{
  return ASN1_INTEGER_get(X509_get_serialNumber((X509 *) x));
}

bool cf_x509_set_serial(X509 * x, long serial)
{
  if ( ASN1_INTEGER_set(X509_get_serialNumber(x),serial) != 1 ) {
    errno = ENOMEM;
    return false;
  }
  return true;
}


char * cf_x509_get_text_entry(const X509_NAME * name, int nid)
{
  const ASN1_STRING * data;
  char * value = NULL;
  int i;

  if ( !name || (i = X509_NAME_get_index_by_NID((X509_NAME *)name, nid, -1)) < 0 ) {
    errno = ENOENT;
  }
  else if ( !(data = X509_NAME_ENTRY_get_data(X509_NAME_get_entry((X509_NAME *)name, i))) ) {
    errno = ENOENT;
  }
  else if ( (value = OPENSSL_malloc(data->length)) ) {
    memcpy(value, data->data, data->length);
    value[data->length] = 0;
  }

  return value;
}

char * cf_x509_get_name(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_name);
}

char * cf_x509_get_common_name(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_commonName);
}

char * cf_x509_get_title(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_title);
}

char * cf_x509_get_altname(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_subject_alt_name);
}

char * cf_x509_get_description(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_description);
}

char * cf_x509_get_country(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_countryName);
}

char * cf_x509_get_state(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_stateOrProvinceName);
}

char * cf_x509_get_city(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_localityName);
}

char * cf_x509_get_company(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_organizationName);
}

char * cf_x509_get_department(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_organizationalUnitName);
}

char * cf_x509_get_domain(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_Domain);
}

char * cf_x509_get_email(const X509_NAME * name)
{
  return cf_x509_get_text_entry(name, NID_pkcs9_emailAddress);
}




bool cf_x509_add_text_entry(X509_NAME * name, int nid, const char * value)
{
  return X509_NAME_add_entry_by_NID(name, nid, MBSTRING_UTF8, (unsigned char * )value, -1, -1, 0) == 1;
}


