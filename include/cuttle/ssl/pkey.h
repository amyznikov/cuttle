/*
 * ssl-pkey.h
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 *
 *  https://wiki.openssl.org/index.php/EVP#Working_with_EVP_PKEYs
 */

//#pragma once

#ifndef __cuttle_ssl_pkey_h__
#define __cuttle_ssl_pkey_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>


#ifdef __cplusplus
extern "C" {
#endif


/**
  Public/Private key pair generation

  Some key types:

  rsa (see rsa_pmeth.c pkey_rsa_ctrl_str)
    rsa_padding_mode: none pkcs1 sslv23 oeap oaep x931 pss
    rsa_pss_saltlen: <integer>
    rsa_keygen_bits: <integer>
    rsa_keygen_pubexp: <BIGNUM>
    rsa_mgf1_md: <digesname>
    rsa_oaep_md: <digesname>
    rsa_oaep_label: <hex>

  dsa (see dsa_pmeth.c pkey_dsa_ctrl_str)
    dsa_paramgen_bits: <integer>
    dsa_paramgen_q_bits: <integer>
    dsa_paramgen_md: <digesname>

  gost94 (see gost_pmeth.c pkey_gost_ctrl94_str)
    paramset: A-D XA-XC

  gost2001 (see gost_pmeth.c pkey_gost_ctrl01_str)
    paramset: A-C 0 XA XB

  gost-mac (see gost_pmeth.c pkey_gost_mac_ctrl_str)
    key: <256-bit key>
    hexkey: <256-bit key>

  dstu4145le dstu4145be (see dstu_pmeth.c dstu_pkey_ctrl_str)
    curve: <uacurve0-uacurve9>
    sbox: <64 bytes hex>

  cmac (see cm_pmeth.c pkey_cmac_ctrl_str)
    key: <key>
    hexkey: <hex-key>
    cipher: <cipherbyname>

  hmac (see hm_pmeth.c pkey_hmac_ctrl_str)
    key: <key>
    hexkey: <hex-key>

  ec id-ecPublicKey (see ec_pmeth.c ec_pkey_meth)
    ec_paramgen_curve: $ openssl ecparam -list_curves
    ec_param_enc: explicit named_curve
    ecdh_kdf_md: <digestbyname>
    ecdh_cofactor_mode: <integer>
*/
EVP_PKEY * cf_pkey_new(const char * ctype, const char * params, EVP_PKEY * pubkey/*=NULL*/);

/** Calls EVP_PKEY_free() */
void cf_pkey_free(EVP_PKEY ** key);

/** Calls EVP_get_digestbynid(EVP_PKEY_get_default_digest_nid(pkey)) */
const EVP_MD * cf_pkey_get_default_md(EVP_PKEY * pkey);


/* EVP_PKEY Read/Write helpers */

bool cf_write_pem_public_key(EVP_PKEY * pkey, const char * filename);
EVP_PKEY * cf_read_pem_public_key(const char * filename);

bool cf_write_pem_public_key_fp(EVP_PKEY * pkey, FILE * fp);
EVP_PKEY * cf_read_pem_public_key_fp(FILE * fp);

bool cf_write_pem_private_key(EVP_PKEY * pkey, const char * filename);
EVP_PKEY * cf_read_pem_private_key(const char * filename);

bool cf_write_pem_private_key_fp(EVP_PKEY * pkey, FILE * fp);
EVP_PKEY * cf_read_pem_private_key_fp(FILE * fp);

bool cf_write_pem_private_key_enc(EVP_PKEY * pkey, const char * filename, const char * enc, const char * psw);
EVP_PKEY * cf_read_pem_private_key_enc(const char * filename, const char * psw);

bool cf_write_pem_private_key_enc_fp(EVP_PKEY * pkey, FILE * fp, const char * enc, const char * psw);
EVP_PKEY * cf_read_pem_private_key_enc_fp(FILE * fp, const char * psw);

char * cf_write_pem_public_key_str(EVP_PKEY * pkey);
EVP_PKEY * cf_read_pem_public_key_str(const char * pem_public_key);

char * cf_write_pem_private_key_str(EVP_PKEY * pkey);
EVP_PKEY * cf_read_pem_private_key_str(const char * pem_private_key);

size_t cf_write_public_key_bits(EVP_PKEY * pkey, /*out*/ uint8_t ** buf);
EVP_PKEY * cf_read_public_key_bits(const uint8_t bits[], size_t cbbits);

size_t cf_write_private_key_bits(EVP_PKEY * pkey, /*out*/ uint8_t ** buf);
EVP_PKEY * cf_read_private_key_bits(const uint8_t bits[], size_t cbbits);

char * cf_write_public_key_hex_str(EVP_PKEY * pkey);
EVP_PKEY * cf_read_public_key_hex_str(const char * hex);

char * cf_write_private_key_hex_str(EVP_PKEY * pkey);
EVP_PKEY * cf_read_private_key_hex_str(const char * hex);

int cf_write_public_key_hex_fp(EVP_PKEY * pkey, FILE * fp);
EVP_PKEY * cf_read_public_key_hex_fp(FILE * fp);

int cf_write_private_key_hex_fp(EVP_PKEY * pkey, FILE * fp);
EVP_PKEY * cf_read_private_key_hex_fp(FILE * fp );

bool cf_write_public_key_hex(EVP_PKEY * pkey, const char * filename);
EVP_PKEY * cf_read_public_key_hex(const char * filename);

bool cf_write_private_key_hex(EVP_PKEY * pkey, const char * filename);
EVP_PKEY * cf_read_private_key_hex(const char * filename);


#ifdef __cplusplus
}
#endif

#endif /* __cuttle_ssl_pkey_h__ */
