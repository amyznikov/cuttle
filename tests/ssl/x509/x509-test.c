/*
 * x509-test.c
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 */

#include <cuttle/ssl/init-ssl.h>
#include <cuttle/ssl/pkey.h>
#include <cuttle/ssl/x509.h>
#include <cuttle/debug.h>
#include <string.h>


// self-signed ca root:
// $ ./x509-test keytype=rsa pkeyout=ca.pem pubkeyout=ca.pub certout=ca.cer ext=keyUsage:critical,keyCertSign,cRLSign ext=basicConstraints:critical,CA:TRUE

// issued:
// $ x509-test keytype=rsa pkeyout=domain.pem certout=domain.cer ca=ca.cer cakey=ca.pem

// see $ man x509v3_config

int main(int argc, char *argv[])
{
  EVP_PKEY * pk = NULL;
  X509 * x509 = NULL;

  X509 * ca = NULL;
  EVP_PKEY * cakey = NULL;

  const char * keytype = NULL;
  const char * keyparams = NULL;

  const char * pkeyout = "stdout";
  const char * pubkeyout = "/dev/null";
  const char * certout = "stdout";

  const char * cafile = NULL;
  const char * cakeyfile = NULL;

  const char * subj_C = "UA";
  const char * subj_ST = "Kiev district";
  const char * subj_L = "Kiev";
  const char * subj_O = "Special-IS";
  const char * subj_OU = "Home";
  const char * subj_CN = "amyznikov";

  const char * conf_path = NULL;

  const EVP_MD * md = NULL;
  const char * digest_name = NULL;

  const int maxext = 128;
  int nbext = 0;
  struct cf_x509_ext ext[maxext];


  /////////////////


  for ( int i = 1; i < argc; ++i ) {

    if ( strcmp(argv[i], "help") == 0 || strcmp(argv[i], "-help") == 0 || strcmp(argv[i], "--help") == 0 ) {

      printf("Usage:\n");
      printf("  x509-test [OPTIONS]\n");
      printf("\n");
      printf("  OPTIONS:\n"
          "   [keytype=<keytype>]\n"
          "   [keyparams=<keyparams>]\n"
          "   [md=<digestname>, see $ openssl list-message-digest-algorithms]\n"
          "   [pkeyout=<output-private-key.pem>]\n"
          "   [pubkeyout=<output-public-key.pem>]\n"
          "   [certout=<output-cert.cer>]\n"
          "   [ca=<ca.cer>]\n"
          "   [cakey=<cakey.pem>]\n"
          "   [C=<Country-Code>]\n"
          "   [ST=<State-Or-Province>]\n"
          "   [L=<Location-Or-City>]\n"
          "   [O=<Organization>]\n"
          "   [OU=<Organization-Unit>]\n"
          "   [CN=<Common-Name>]\n"
          "   [ext=sn:value]\n"
          " \n");

      printf("keytype examples: \n\n");
      printf("  rsa (see rsa_pmeth.c pkey_rsa_ctrl_str)\n");
      printf("     rsa_padding_mode: none pkcs1 sslv23 oeap oaep x931 pss\n");
      printf("     rsa_pss_saltlen: <integer>\n");
      printf("     rsa_keygen_bits: <integer>\n");
      printf("     rsa_keygen_pubexp: <BIGNUM>\n");
      printf("     rsa_mgf1_md: <digesname>\n");
      printf("     rsa_oaep_md: <digesname>\n");
      printf("     rsa_oaep_label: <hex>\n");
      printf("\n");
      printf("  dsa (see dsa_pmeth.c pkey_dsa_ctrl_str)\n");
      printf("     dsa_paramgen_bits: <integer>\n");
      printf("     dsa_paramgen_q_bits: <integer>\n");
      printf("     dsa_paramgen_md: <digesname>\n");
      printf("\n");
      printf("  gost94 (see gost_pmeth.c pkey_gost_ctrl94_str)\n");
      printf("     paramset: A-D XA-XC\n");
      printf("\n");
      printf("  gost2001 (see gost_pmeth.c pkey_gost_ctrl01_str)\n");
      printf("     paramset: A-C 0 XA XB\n");
      printf("\n");
      printf("     gost-mac (see gost_pmeth.c pkey_gost_mac_ctrl_str)\n");
      printf("     key: <256-bit key>\n");
      printf("     hexkey: <256-bit key>\n");
      printf("\n");
      printf("  dstu4145le dstu4145be (see dstu_pmeth.c dstu_pkey_ctrl_str)\n");
      printf("     curve: <uacurve0-uacurve9>\n");
      printf("     sbox: <64 bytes hex>\n");
      printf("\n");
      printf("  ec id-ecPublicKey (see ec_pmeth.c ec_pkey_meth)\n");
      printf("     ec_paramgen_curve: see $ openssl ecparam -list_curves\n");
      printf("     ec_param_enc: explicit named_curve\n");
      printf("     ecdh_kdf_md: <digestbyname>\n");
      printf("     ecdh_cofactor_mode: <integer> \n");
      printf("\n");

      return 0;
    }

    if ( strncmp(argv[i], "keytype=", 8) == 0 ) {
      keytype = argv[i] + 8;
    }
    else if ( strncmp(argv[i], "keyparams=", 10) == 0 ) {
      keyparams = argv[i] + 10;
    }
    else if ( strncmp(argv[i], "md=", 3) == 0 ) {
      digest_name = argv[i] + 3;
    }
    else if ( strncmp(argv[i], "pkeyout=", 8) == 0 ) {
      pkeyout = argv[i] + 8;
    }
    else if ( strncmp(argv[i], "pubkeyout=", 10) == 0 ) {
      pubkeyout = argv[i] + 10;
    }
    else if ( strncmp(argv[i], "certout=", 8) == 0 ) {
      certout = argv[i] + 8;
    }
    else if ( strncmp(argv[i], "ca=", 3) == 0 ) {
      cafile = argv[i] + 3;
    }
    else if ( strncmp(argv[i], "cakey=", 6) == 0 ) {
      cakeyfile = argv[i] + 6;
    }
    else if ( strncmp(argv[i], "C=", 2) == 0 ) {
      subj_C = argv[i] + 2;
    }
    else if ( strncmp(argv[i], "ST=", 3) == 0 ) {
      subj_ST = argv[i] + 3;
    }
    else if ( strncmp(argv[i], "L=", 2) == 0 ) {
      subj_L = argv[i] + 2;
    }
    else if ( strncmp(argv[i], "O=", 2) == 0 ) {
      subj_O = argv[i] + 2;
    }
    else if ( strncmp(argv[i], "OU=", 3) == 0 ) {
      subj_OU = argv[i] + 3;
    }
    else if ( strncmp(argv[i], "CN=", 3) == 0 ) {
      subj_CN = argv[i] + 3;
    }
    else if ( strncmp(argv[i], "config=", 7) == 0 ) {
      conf_path = argv[i] + 7;
    }
    else if ( strncmp(argv[i], "ext=", 4) == 0 ) {
      char * s, * v;
      if ( nbext >= maxext ) {
        fprintf(stderr, "too many extentions\n");
        return 1;
      }
      if ( !*(s = argv[i] + 4) || !(v = strchr(s + 1, ':')) ) {
        fprintf(stderr, "syntax error in '%s'\n", argv[i]);
        return 1;
      }

      *v = 0;

      if ( !(ext[nbext].nid =  OBJ_sn2nid(s)) ) {
        fprintf(stderr, "OBJ_sn2nid(%s) fails: %s\n", s, argv[i]);
        return 1;
      }

      ext[nbext].value = v + 1;
      ++nbext;
    }
    else {
      fprintf(stderr, "Invalid argument %s\n", argv[i]);
      return 1;
    }
  }


  cf_set_loglevel(CF_LOG_DEBUG);
  cf_set_logfilename("stderr");


  /////////////////


  if ( !cf_ssl_initialize(conf_path) ) {
    fprintf(stderr, "cf_ssl_initialize() fails\n");
    ERR_print_errors_fp(stderr);
    goto end;
  }

  /////////////////


  if ( digest_name && *digest_name && !(md = EVP_get_digestbyname(digest_name)) ) {
    fprintf(stderr, "EVP_get_digestbyname(%s) fails\n", digest_name);
    ERR_print_errors_fp(stderr);
    goto end;
  }

  /////////////////


  if ( cafile && !(ca = cf_read_pem_x509(cafile)) ) {
    fprintf(stderr, "cf_read_pem_x509(%s) fails\n", cafile);
    ERR_print_errors_fp(stderr);
    goto end;
  }


  if ( cakeyfile && !(cakey = cf_read_pem_private_key(cakeyfile)) ) {
    fprintf(stderr, "cf_read_pem_private_key(%s) fails\n", cakeyfile);
    ERR_print_errors_fp(stderr);
    goto end;
  }


  /////////////////

  x509 = cf_x509_new(&pk, &(struct cf_x509_create_args ) {

      .md = md,
      .serial = rand(),

      .ca = {
        .cert = ca,
        .pkey = cakey,
      },

      .keygen = {
        .keytype = keytype,
        .params = keyparams,
      },

      .valididy = {
        .notBefore.time = 0,
        .notAfter.period.days = 365,
      },

      .subj = {
        .country = subj_C,
        .state = subj_ST,
        .city = subj_L,
        .company = subj_O,
        .department = subj_OU,
        .common_name = subj_CN,
        .email = "andrey.myznikov@gmail.com",
      },

      .ext = ext,
      .nbext = nbext
    });


  if ( !x509 ) {
    fprintf(stderr, "cf_ssl_x509_new() fails\n");
    ERR_print_errors_fp(stderr);
    goto end;
  }

  if ( !cf_write_pem_x509(x509, certout) ) {
    fprintf(stderr, "cf_write_pem_x509(%s) fails\n", certout);
    ERR_print_errors_fp(stderr);
    goto end;
  }

  if ( !cf_write_pem_private_key(pk, pkeyout) ) {
    fprintf(stderr, "cf_write_pem_private_key(%s) fails\n", pkeyout);
    ERR_print_errors_fp(stderr);
    goto end;
  }

  if ( !cf_write_pem_public_key(pk, pubkeyout) ) {
    fprintf(stderr, "cf_write_pem_public_key(%s) fails\n", pubkeyout);
    ERR_print_errors_fp(stderr);
    goto end;
  }


end:

  return 0;
}

