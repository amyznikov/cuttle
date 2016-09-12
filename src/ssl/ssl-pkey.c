/*
 * ssl-pkey.c
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include <cuttle/ssl/error.h>
#include <cuttle/ssl/pkey.h>
#include <cuttle/hexbits.h>
#include <string.h>
#include <ctype.h>
#include "getfp.h"

static inline const EVP_CIPHER * cf_cipher_by_name(const char * cname)
{
  return cname && *cname ? EVP_get_cipherbyname(cname) : NULL;
}

struct param {
  const char * key;
  const char * value;
};

static int parsekeyparams(char s[], struct param params[], int maxparams)
{
  static const char delims[] = " \t\n";
  int nparams = 0;
  char * p, *v;

  p = strtok(s, delims);
  while ( p && nparams < maxparams ) {

    if ( !(v = strchr(p + 1, ':')) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "Syntax error in '%s'", p);
      break;
    }

    *v++ = 0;

    params[nparams].key = p;
    params[nparams].value = v;
    ++nparams;

    p = strtok(NULL, delims);
  }

  if ( nparams == maxparams ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "Too many parameters");
  }

  return p ? -1 : nparams;
}


// https://wiki.openssl.org/index.php/EVP_Key_and_Parameter_Generation
EVP_PKEY * cf_pkey_new(const char * ctype, const char * params, EVP_PKEY * pubkey)
{
  EVP_PKEY * key = NULL;
  EVP_PKEY_CTX * paramgen_ctx = NULL;
  EVP_PKEY * key_params = NULL;
  EVP_PKEY_CTX * keygen_ctx = NULL;
  int type = 0;
  int status;

  char param_buf[params ? strlen(params) + 1 : 1];
  struct param param_array[128];
  int nparams = 0;

  bool fok = false;

  if ( ctype ) {

    /* see obj_dat.h  obj_mac.h */
    if ( strcasecmp(ctype, "rsa") == 0 ) {
      ctype = LN_rsaEncryption;
    }
    else if ( strcmp(ctype, "dsa") == 0 ) {
      ctype = LN_dsa;
    }
    else if ( strcasecmp(ctype, "ec") == 0 ) {
      ctype = SN_X9_62_id_ecPublicKey;
    }

    if ( !(type = OBJ_sn2nid(ctype)) && !(type = OBJ_ln2nid(ctype)) ) {
      CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Unknown key type '%s' specified", ctype);
      return 0;
    }
  }

  if ( pubkey ) {
    if ( !type ) {
      type = EVP_PKEY_id(pubkey);
    }
    else if ( type != EVP_PKEY_id(pubkey) ) {
      CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "ctype=%d not match pubkey type=%d", type, EVP_PKEY_id(pubkey));
      goto end;
    }

    if ( !(paramgen_ctx = EVP_PKEY_CTX_new(pubkey, NULL)) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "paramgen_ctx = EVP_PKEY_CTX_new_id() fails");
      goto end;
    }
  }
  else {
    if ( !type ) {
      CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Key type not specified");
      goto end;
    }
    if ( !(paramgen_ctx = EVP_PKEY_CTX_new_id(type, NULL)) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "paramgen_ctx = EVP_PKEY_CTX_new_id() fails");
      goto end;
    }
  }

  /* Set the parameters */
  if ( params ) {
    strcpy(param_buf, params);
    if ( (nparams = parsekeyparams(param_buf, param_array, sizeof(param_array) / sizeof(param_array[0]))) < 0 ) {
      goto end;
    }
  }

  if ( (status = EVP_PKEY_paramgen_init(paramgen_ctx)) == -2 ) {
    EVP_PKEY_CTX_free(paramgen_ctx), paramgen_ctx = NULL;
    ERR_clear_error(); /* seems operation is not supported for this keytype, skip this step */
  }
  else if ( status != 1 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_paramgen_init() fails: status=%d", status);
    goto end;
  }
  else {
    for ( int i = 0; i < nparams; ++i ) {
      const char * key = param_array[i].key, *value = param_array[i].value;
      if ( (status = EVP_PKEY_CTX_ctrl_str(paramgen_ctx, key, value)) <= 0 ) {
        CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_CTX_ctrl_str(paramgen, '%s:%s') fails: status=%d", key, value, status);
        goto end;
      }
    }
    /* Generate parameters */
    if ( EVP_PKEY_paramgen(paramgen_ctx, &key_params) != 1 ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_paramgen() fails");
      goto end;
    }
  }

  /* Key Generation */
  if ( key_params ) {
    if ( !(keygen_ctx = EVP_PKEY_CTX_new(key_params, NULL)) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_CTX_new(key_params) fails");
      goto end;
    }
  }
  else if ( pubkey ) {
    if ( !(keygen_ctx = EVP_PKEY_CTX_new(pubkey, NULL)) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_CTX_new(pubkey) fails");
      goto end;
    }
  }
  else if ( !(keygen_ctx = EVP_PKEY_CTX_new_id(type, NULL)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_CTX_new_id(type=%d) fails", type);
    goto end;
  }

  if ( EVP_PKEY_keygen_init(keygen_ctx) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_keygen_init() fails");
    goto end;
  }

  if ( !key_params ) {
    for ( int i = 0; i < nparams; ++i ) {
      const char * key = param_array[i].key, *value = param_array[i].value;
      if ( (status = EVP_PKEY_CTX_ctrl_str(keygen_ctx, key, value)) <= 0 ) {
        CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_CTX_ctrl_str(keygen, '%s:%s') fails: status=%d", key, value, status);
        goto end;
      }
    }
  }

  /* Finally Generate the key */
  if ( (status = EVP_PKEY_keygen(keygen_ctx, &key)) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_keygen() fails: status=%d", status);
    goto end;
  }

  fok = true;

end : ;

  if ( keygen_ctx ) {
    EVP_PKEY_CTX_free(keygen_ctx);
  }

  if ( paramgen_ctx ) {
    EVP_PKEY_CTX_free(paramgen_ctx);
  }

  cf_pkey_free(&key_params);

  if ( !fok ) {
    cf_pkey_free(&key);
  }

  return key;
}

void cf_pkey_free(EVP_PKEY ** key)
{
  if ( key && *key ) {
    EVP_PKEY_free(*key);
    *key = NULL;
  }
}

const EVP_MD * cf_pkey_get_default_md(EVP_PKEY * pkey)
{
  const EVP_MD * md = NULL;
  int nid = 0;

  if ( EVP_PKEY_get_default_digest_nid(pkey, &nid) > 0 ) {
    md = EVP_get_digestbynid(nid);
  }

  return md;
}

bool cf_write_pem_public_key_fp(EVP_PKEY * pkey, FILE * fp)
{
  return PEM_write_PUBKEY(fp, pkey) == 1;
}

EVP_PKEY * cf_read_pem_public_key_fp(FILE * fp)
{
  EVP_PKEY * key = NULL;
  bool fOk = false;

  if ( !(key = EVP_PKEY_new()) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_new() fails");
    goto end;
  }

  if ( !PEM_read_PUBKEY(fp, &key, NULL, NULL) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_read_PUBKEY() fails");
    goto end;
  }

  fOk = true;

end : ;

  if ( !fOk && key != NULL ) {
    EVP_PKEY_free(key);
    key = NULL;
  }

  return key;
}

bool cf_write_pem_public_key(EVP_PKEY * pkey, const char * fname)
{
  FILE * fp = NULL;
  bool fok = false;

  if ( (fp = cf_getfp(fname, "w", &fok)) ) {
    fok = cf_write_pem_public_key_fp(pkey, fp);
    cf_closefp(&fp);
  }

  return fok;
}

EVP_PKEY * cf_read_pem_public_key(const char * fname)
{
  FILE * fp = NULL;
  EVP_PKEY * key = NULL;

  if ( (fp = cf_getfp(fname, "r", NULL)) ) {
    if ( !(key = cf_read_pem_public_key_fp(fp)) ) {
      CF_SSL_ERR(CF_SSL_ERR_STDIO, "cf_read_pem_public_key_fp() fails: %s", strerror(errno));
    }
    cf_closefp(&fp);
  }

  return key;
}

bool cf_write_pem_private_key(EVP_PKEY * pkey, const char * filename)
{
  return cf_write_pem_private_key_enc(pkey, filename, NULL, NULL);
}

EVP_PKEY * cf_read_pem_private_key(const char * filename)
{
  return cf_read_pem_private_key_enc(filename, NULL);
}

bool cf_write_pem_private_key_fp(EVP_PKEY * pkey, FILE * fp)
{
  return cf_write_pem_private_key_enc_fp(pkey, fp, NULL, NULL);
}

EVP_PKEY * cf_read_pem_private_key_fp(FILE * fp)
{
  return cf_read_pem_private_key_enc_fp(fp, NULL);
}

bool cf_write_pem_private_key_enc_fp(EVP_PKEY * pkey, FILE * fp, const char * enc_name, const char * psw)
{
  const EVP_CIPHER * enc = NULL;
  int fOk = false;

  if ( !psw ) {
    if ( enc_name ) {
      CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "no passphrase specified for cipher '%s'", enc_name);
      goto end;
    }
  }
  else {
    if ( !enc_name ) {
      enc_name = "dstu28147-cfb";
    }
    if ( !(enc = cf_cipher_by_name(enc_name)) ) {
      CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "cf_cipher_by_name(%s) fails", enc_name);
      goto end;
    }
  }

  if ( PEM_write_PrivateKey(fp, pkey, enc, NULL, 0, NULL, (void*) psw) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_write_PrivateKey() fails");
    goto end;
  }

  fOk = true;

  end : ;

  return fOk;
}

EVP_PKEY * cf_read_pem_private_key_enc_fp(FILE * fp, const char * psw)
{
  EVP_PKEY * key = NULL;
  bool fOk = false;

  if ( !(key = EVP_PKEY_new()) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_new() fails");
    goto end;
  }

  if ( !PEM_read_PrivateKey(fp, &key, NULL, (void*) psw) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_read_PrivateKey() fails");
    goto end;
  }

  fOk = true;

  end : ;

  if ( !fOk && key != NULL ) {
    EVP_PKEY_free(key);
    key = NULL;
  }

  return key;

}

bool cf_write_pem_private_key_enc(EVP_PKEY * pkey, const char * fname, const char * enc, const char * psw)
{
  FILE * fp = NULL;
  bool fok = false;

  if ( (fp = cf_getfp(fname, "w", &fok)) ) {
    fok = cf_write_pem_private_key_enc_fp(pkey, fp, enc, psw);
    cf_closefp(&fp);
  }

  return fok;
}

EVP_PKEY * cf_read_pem_private_key_enc(const char * fname, const char * psw)
{
  FILE * fp = NULL;
  EVP_PKEY * key = NULL;

  if ( (fp = cf_getfp(fname, "r", NULL)) ) {
    if ( !PEM_read_PrivateKey(fp, &key, NULL, (void*) psw) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_read_PrivateKey() fails");
    }
    cf_closefp(&fp);
  }

  return key;
}

char * cf_write_pem_public_key_str(EVP_PKEY * key)
{
  char * outbuf = NULL;
  BIO * bio = NULL;
  BUF_MEM * mem = NULL;

  if ( !(bio = BIO_new(BIO_s_mem())) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new(BIO_s_mem()) fails");
    goto end;
  }

  if ( PEM_write_bio_PUBKEY(bio, key) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_write_bio_PUBKEY() fails");
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

end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  return outbuf;
}

EVP_PKEY * cf_read_pem_public_key_str(const char * public_key)
{
  EVP_PKEY * key = NULL;
  BIO * bio = NULL;

  int fOk = 0;

  if ( !public_key || !*public_key ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "No public key string provided");
    goto end;
  }

  if ( !(key = EVP_PKEY_new()) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "EVP_PKEY_new() fails");
    goto end;
  }

  if ( !(bio = BIO_new_mem_buf((void*) public_key, -1)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new_mem_buf(public_key) fails");
    goto end;
  }

  if ( !PEM_read_bio_PUBKEY(bio, &key, NULL, NULL) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_read_bio_PUBKEY() fails");
    goto end;
  }

  fOk = 1;

end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  if ( key && !fOk ) {
    EVP_PKEY_free(key), key = NULL;
  }

  return key;
}

char * cf_write_pem_private_key_str(EVP_PKEY * key)
{
  char * outbuf = NULL;
  BIO * bio = NULL;
  BUF_MEM * mem = NULL;

  if ( !(bio = BIO_new(BIO_s_mem())) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new(BIO_s_mem()) fails");
    goto end;
  }

  if ( PEM_write_bio_PrivateKey(bio, key, NULL, NULL, 0, NULL, NULL) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_write_bio_PrivateKey() fails");
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

end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  return outbuf;
}

EVP_PKEY * cf_read_pem_private_key_str(const char * private_key)
{
  EVP_PKEY * key = NULL;
  BIO * bio = NULL;

  bool fOk = false;

  if ( !private_key || !*private_key ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "No private key string provided");
    goto end;
  }

  if ( !(bio = BIO_new_mem_buf(private_key, -1)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new_mem_buf(private_key) fails");
    goto end;
  }

  if ( !PEM_read_bio_PrivateKey(bio, &key, NULL, NULL) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "PEM_read_bio_PrivateKey() fails");
    goto end;
  }

  fOk = true;

end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  if ( key && !fOk ) {
    EVP_PKEY_free(key);
    key = NULL;
  }

  return key;
}

size_t cf_write_public_key_bits(EVP_PKEY * pkey, uint8_t ** buf)
{
  BIO * bio = NULL;
  BUF_MEM * mem = NULL;
  size_t outlen = 0;

  *buf = NULL;

  if ( !(bio = BIO_new(BIO_s_mem())) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new(BIO_s_mem()) fails");
    goto end;
  }

  if ( i2d_PUBKEY_bio(bio, pkey) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "i2d_PUBKEY_bio() fails");
    goto end;
  }

  BIO_get_mem_ptr(bio, &mem);
  if ( !mem ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_get_mem_ptr() fails");
    goto end;
  }

  if ( !(*buf = OPENSSL_malloc(mem->length)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "OPENSSL_malloc(%zu) fails", mem->length);
    goto end;
  }

  memcpy(*buf, mem->data, mem->length);
  outlen = mem->length;

end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  return outlen;
}

EVP_PKEY * cf_read_public_key_bits(const uint8_t buf[], size_t cbbuf)
{
  EVP_PKEY * pkey = NULL;
  BIO * bio = NULL;

  if ( !(bio = BIO_new_mem_buf((void*) buf, cbbuf)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new_mem_buf() fails");
    goto end;
  }

  d2i_PUBKEY_bio(bio, &pkey);
  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "d2i_PUBKEY_bio() fails");
    goto end;
  }

end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  return pkey;
}

size_t cf_write_private_key_bits(EVP_PKEY * pkey, uint8_t ** buf)
{
  BIO * bio = NULL;
  BUF_MEM * mem = NULL;
  size_t outlen = 0;

  *buf = NULL;

  if ( !(bio = BIO_new(BIO_s_mem())) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new(BIO_s_mem()) fails");
    goto end;
  }

  if ( i2d_PrivateKey_bio(bio, pkey) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "i2d_PrivateKey_bio() fails");
    goto end;
  }

  BIO_get_mem_ptr(bio, &mem);
  if ( !mem ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_get_mem_ptr() fails");
    goto end;
  }

  if ( !(*buf = OPENSSL_malloc(mem->length)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "OPENSSL_malloc(%zu) fails", mem->length);
    goto end;
  }

  memcpy(*buf, mem->data, mem->length);
  outlen = mem->length;

  end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  return outlen;
}

EVP_PKEY * cf_read_private_key_bits(const uint8_t buf[], size_t cbbuf)
{
  EVP_PKEY * pkey = NULL;
  BIO * bio = NULL;

  if ( !(bio = BIO_new_mem_buf((void*) buf, cbbuf)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "BIO_new_mem_buf() fails");
    goto end;
  }

  d2i_PrivateKey_bio(bio, &pkey);
  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "d2i_PrivateKey_bio() fails");
    goto end;
  }

end : ;

  if ( bio ) {
    BIO_free(bio);
  }

  return pkey;
}

char * cf_write_public_key_hex_str(EVP_PKEY * pkey)
{
  uint8_t * bits = NULL;
  int cbbits = 0;
  char * hex = NULL;

  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Invalid argument: key=%p", pkey);
    goto end;
  }

  if ( (cbbits = cf_write_public_key_bits(pkey, &bits)) <= 0 ) {
    goto end;
  }

  if ( !(hex = OPENSSL_malloc(cbbits * 2 + 1)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "OPENSSL_malloc(%d) fails", cbbits * 2 + 1);
    goto end;
  }

  cf_bits2hex(bits, cbbits, hex);

end : ;

  OPENSSL_free(bits);

  return hex;
}

EVP_PKEY * cf_read_public_key_hex_str(const char * hex)
{
  size_t keylen;
  uint8_t * bits = NULL;

  if ( !hex || !*hex ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "No key string provided");
    return NULL;
  }

  bits = alloca(keylen = (strlen(hex) / 2 + 2));
  if ( !(keylen = cf_hex2bits(hex, bits, keylen)) ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "hex2bits() fails");
    return NULL;
  }

  return cf_read_public_key_bits(bits, keylen);
}

char * cf_write_private_key_hex_str(EVP_PKEY * pkey)
{
  uint8_t * bits = NULL;
  int cbbits = 0;
  char * hex = NULL;

  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Invalid argument: key=%p", pkey);
    goto end;
  }

  if ( (cbbits = cf_write_private_key_bits(pkey, &bits)) <= 0 ) {
    goto end;
  }

  if ( !(hex = OPENSSL_malloc(cbbits * 2 + 1)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "OPENSSL_malloc(%d) fails", cbbits * 2 + 1);
    goto end;
  }

  cf_bits2hex(bits, cbbits, hex);

end : ;

  OPENSSL_free(bits);

  return hex;
}

EVP_PKEY * cf_read_private_key_hex_str(const char * hex)
{
  uint8_t * bits = NULL;
  size_t cbbits;

  if ( !hex || !*hex ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "No key string provided");
    return NULL;
  }

  bits = alloca(cbbits = (strlen(hex) / 2 + 2));
  if ( !(cbbits = cf_hex2bits(hex, bits, cbbits)) ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "hex2bits() fails");
    return NULL;
  }

  return cf_read_private_key_bits(bits, cbbits);
}

int cf_write_public_key_hex_fp(EVP_PKEY * pkey, FILE * fp)
{
  uint8_t * bits = NULL;
  int cbbits = 0;
  char * hex = NULL;
  int fOk = 0;

  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Invalid argument key=NULL");
    goto end;
  }

  if ( (cbbits = cf_write_public_key_bits(pkey, &bits)) <= 0 ) {
    goto end;
  }

  if ( fprintf(fp, "%s", cf_bits2hex(bits, cbbits, hex = alloca(cbbits * 2 + 1))) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "fprintf(fp=%p) fails: %s", fp, strerror(errno));
    goto end;
  }

  fOk = 1;

end : ;

  OPENSSL_free(bits);

  return fOk;
}

EVP_PKEY * cf_read_public_key_hex_fp(FILE * fp)
{
  EVP_PKEY * pkey = NULL;
  char * buf = NULL;
  size_t bufsize = 0;
  size_t hexsize = 0;
  int syntax_error = 0;
  int ch;

  while ( (ch = fgetc(fp)) != EOF && !isspace(ch) ) {

    if ( !strchr("0123456789ABCDEFabcdef", ch) ) {
      syntax_error = 1;
      break;
    }

    if ( hexsize + 1 >= bufsize ) {
      buf = realloc(buf, bufsize += 1024);
    }

    buf[hexsize++] = ch;
    buf[hexsize] = 0;
  }

  if ( syntax_error ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Syntax error in input stream");
  }
  else {
    pkey = cf_read_public_key_hex_str(buf);
  }

  free(buf);

  return pkey;
}

int cf_write_private_key_hex_fp(EVP_PKEY * pkey, FILE * fp)
{
  uint8_t * bits = NULL;
  int cbbits = 0;
  char * hex = NULL;
  int fOk = 0;

  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Invalid argument key=NULL");
    goto end;
  }

  if ( (cbbits = cf_write_private_key_bits(pkey, &bits)) <= 0 ) {
    goto end;
  }

  if ( fprintf(fp, "%s", cf_bits2hex(bits, cbbits, hex = alloca(cbbits * 2 + 1))) <= 0 ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "fprintf(fp=%p) fails: %s", fp, strerror(errno));
    goto end;
  }

  fOk = 1;

end : ;

  OPENSSL_free(bits);

  return fOk;
}

EVP_PKEY * cf_read_private_key_hex_fp(FILE * fp)
{
  EVP_PKEY * pkey = NULL;
  char * buf = NULL;
  size_t bufsize = 0;
  size_t hexsize = 0;
  int syntax_error = 0;
  int ch;

  while ( (ch = fgetc(fp)) != EOF && !isspace(ch) ) {
    if ( !strchr("0123456789ABCDEFabcdef", ch) ) {
      syntax_error = 1;
      break;
    }

    if ( hexsize + 1 >= bufsize ) {
      buf = realloc(buf, bufsize += 1024);
    }

    buf[hexsize++] = ch;
    buf[hexsize] = 0;
  }

  if ( syntax_error ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Syntax error in input stream");
  }
  else {
    pkey = cf_read_private_key_hex_str(buf);
  }

  free(buf);

  return pkey;
}

bool cf_write_public_key_hex(EVP_PKEY * pkey, const char * filename)
{
  FILE * fp = NULL;
  bool fok = false;

  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Invalid argument key=NULL");
  }
  else if ( (fp = cf_getfp(filename, "w", &fok)) ) {
    if ( (fok = cf_write_public_key_hex_fp(pkey, fp)) ) {
      fputc('\n', fp);
    }
    cf_closefp(&fp);
  }

  return fok;
}

EVP_PKEY * cf_read_public_key_hex(const char * fname)
{
  FILE * fp = NULL;
  EVP_PKEY * pkey = NULL;

  if ( (fp = cf_getfp(fname, "r", NULL)) ) {
    pkey = cf_read_public_key_hex_fp(fp);
    cf_closefp(&fp);
  }

  return pkey;
}

bool cf_write_private_key_hex(EVP_PKEY * pkey, const char * fname)
{
  FILE * fp = NULL;
  bool fok = 0;

  if ( !pkey ) {
    CF_SSL_ERR(CF_SSL_ERR_INVALID_ARG, "Invalid key: NULL");
  }
  else if ( (fp = cf_getfp(fname, "w", &fok)) ) {
    if ( (fok = cf_write_private_key_hex_fp(pkey, fp)) ) {
      fputc('\n', fp);
    }
    cf_closefp(&fp);
  }

  return fok;
}

EVP_PKEY * cf_read_private_key_hex(const char * fname)
{
  FILE * fp = NULL;
  EVP_PKEY * pkey = NULL;

  if ( (fp = cf_getfp(fname, "r", NULL)) ) {
    pkey = cf_read_private_key_hex_fp(fp);
    cf_closefp(&fp);
  }

  return pkey;
}
