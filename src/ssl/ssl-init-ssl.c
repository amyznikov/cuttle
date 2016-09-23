
/*
 * init-ssl.c
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 *
 *  https://wiki.openssl.org/index.php/Libcrypto_API
 *
 *  https://github.com/iSECPartners/ssl-conservatory
 */
#define _GNU_SOURCE

#include <cuttle/debug.h>
#include <cuttle/ssl/init-ssl.h>
#include <cuttle/ssl/error.h>
#include <openssl/crypto.h>
#include <openssl/engine.h>
#include <openssl/ssl.h>
#include <string.h>
#include <pthread.h>


#define UNUSED(x) (void)(x)


/* defined in ssl-error.c */
extern void cf_init_ssl_error_strings(void);


/*********************************************************************************************************************
 * OpenSSL verstion ssl-int compiled with
 */

int cf_get_opennssl_version_number(void)
{
 return OPENSSL_VERSION_NUMBER;
}

const char * cf_get_opennssl_version_string(void)
{
  return OPENSSL_VERSION_TEXT;
}

const char * cf_get_runtime_opennssl_version_string(void)
{
  return SSLeay_version(SSLEAY_VERSION);
}


/*********************************************************************************************************************
 * OpenSSL in threaded environment
 *  @see http://www.openssl.org/support/faq.html#PROG1
 *  @see http://www.openssl.org/docs/crypto/threads.html
 */
static pthread_rwlock_t * cf_ssl_locks;

static void cf_ssl_thread_id_callback(CRYPTO_THREADID * p)
{
  p->ptr = NULL;
  p->val = pthread_self();
}

static void cf_ssl_pthreads_locking_callback(int mode, int type, const char *file, int line)
{
  UNUSED(file), UNUSED(line);

  if ( !(mode & CRYPTO_LOCK) ) {
    // fprintf(stderr, "unlock %3d from %s:%d", type, file, line);
    pthread_rwlock_unlock(&cf_ssl_locks[type]);
  }
  else if ( mode & CRYPTO_WRITE ) {
    // fprintf(stderr, "wrlock %3d from %s:%d", type, file, line);
    pthread_rwlock_wrlock(&cf_ssl_locks[type]);
  }
  else if ( mode & CRYPTO_READ ) {
    // fprintf(stderr, "rdlock %3d from %s:%d", type, file, line);
    pthread_rwlock_rdlock(&cf_ssl_locks[type]);
  }
  else {
    //errmsg("invalid lock mode 0x%X type=%d from %s:%d", mode, type, file, line);
  }
}

static void cf_ssl_thread_setup(void)
{
  int i, n;

  n = CRYPTO_num_locks();
  cf_ssl_locks = OPENSSL_malloc(n * sizeof(*cf_ssl_locks));
  for ( i = 0; i < n; i++ ) {
    pthread_rwlock_init(&cf_ssl_locks[i], NULL);
  }

  CRYPTO_THREADID_set_callback(cf_ssl_thread_id_callback);
  CRYPTO_set_locking_callback(cf_ssl_pthreads_locking_callback);
}

//static void cf_ssl_thread_cleanup(void)
//{
//  int i, n;
//  CRYPTO_set_locking_callback(NULL);
//  for ( i = 0, n = CRYPTO_num_locks(); i < n; i++ ) {
//    pthread_rwlock_destroy(&cf_ssl_locks[i]);
//  }
//  OPENSSL_free(cf_ssl_locks);
//}


static bool cf_ssl_load_config(const char * openssl_conf)
{
  /*
  To load extra engines add the folloving lines (as an example) to file openssl_conf:

  #The last line in default section (just before first [...]):

  cuttlessl_conf = cuttlessl_def

  #at the end of file:

  #####################################################################
  [cuttlessl_def]
  engines = engine_section

  #####################################################################
  [engine_section]
  gost = gost_section
  dstu = dstu_section

  [gost_section]
  engine_id = gost
  dynamic_path = /usr/local/lib/engines/libgost.so
  default_algorithms = ALL
  CRYPT_PARAMS = id-Gost28147-89-CryptoPro-A-ParamSet

  [dstu_section]
  engine_id = dstu
  dynamic_path = /usr/local/lib/engines/libdstu.so
  default_algorithms = ALL

  */

  /*
  To make this working, add something like this at the end of openssl.cnf:

  [dstu_section]
  engine_id = dstu
  dynamic_path = /usr/local/lib/engines/libdstu.so
  default_algorithms = ALL

  [$cuttlessl_rand]

  # engine_id, that specifies default engine for random number generation.
  # NOTE: If this variable is used, all default_algorithms of this engine will
  # be available in application even if it is not included into [engine_section].
  def_rand_engine = ${dstu_section::engine_id}
  */

  CONF * def_config = NULL;
  ENGINE * rand_engine = NULL;
  long err = 0;
  bool rand_changed = false;
  bool loaded = false;
  char * rand_engineID = NULL;
  char * conf_path;

  if (!openssl_conf) {
    conf_path =  CONF_get1_default_config_file();
  }
  else {
    conf_path = openssl_conf;
  }


  if ( !(def_config = NCONF_new(NULL)) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "NCONF_new() fails");
    goto end;
  }

  if (!NCONF_load(def_config, conf_path, &err))  {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "NCONF_load() fails");
    goto end;
  }

  if( CONF_modules_load(def_config, "cuttlessl_conf", 0) <= 0)  {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "CONF_modules_load(%s, 'cuttlessl_conf', NULL)) fails", conf_path);
    goto end;
  }

  CF_INFO("Initialized using the following config: %s", conf_path);
  loaded = true;

  /* set rand method */
  if( !(rand_engineID = _CONF_get_string(def_config, "cuttlessl_rand", "def_rand_engine")))  {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "Cannot find $cuttlessl_rand::defRandEngine value in config file.");
    goto end;
  }

  rand_engine = ENGINE_by_id(rand_engineID);
  if(!rand_engine) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "ENGINE_by_id() fails");
    goto end;
  }

  if(!ENGINE_init(rand_engine)) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "ENGINE_init() fails");
    goto end;
  }

  if( !ENGINE_get_RAND(rand_engine) ) {
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "ENGINE_get_RAND() fails, engine_id = %s", rand_engineID);
    goto end;
  }

  if(! ENGINE_register_RAND(rand_engine)){
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "ENGINE_register_RAND() fails, engine_id = %s", rand_engineID);
    goto end;
  }

  if(! ENGINE_set_default(rand_engine, ENGINE_METHOD_RAND)){
    CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "ENGINE_set_default(ENGINE_METHOD_RAND) fails, engine_id = %s", rand_engineID);
    goto end;
  }

  rand_changed = true;
  CF_INFO("Engine '%s' used as default for random number generation.", rand_engineID);

  end:
  if(!loaded) {
    CF_INFO("Error occurred while reading configuration file %s. No config was used.", conf_path);
  }
  if(!rand_changed) {
    CF_INFO("Cannot set %s engine as default for random number generation. Method was not changed.", rand_engineID);
  }
  ENGINE_free(rand_engine);
  CONF_modules_free();
  NCONF_free(def_config);
  return loaded;
}

bool cf_ssl_initialize(const char * openssl_conf)
{
  static bool is_initialized = false;

  if ( !is_initialized ) {

    SSL_library_init();

    OPENSSL_load_builtin_modules();
    ENGINE_load_builtin_engines();

    ERR_load_crypto_strings();
    cf_init_ssl_error_strings();

    if ( !cf_ssl_load_config(openssl_conf) ) {
      CF_SSL_ERR(CF_SSL_ERR_OPENSSL, "cf_ssl_load_config() fails");
      goto end;
    }

    OpenSSL_add_all_ciphers();
    OpenSSL_add_all_digests();
    OpenSSL_add_all_algorithms();

    cf_ssl_thread_setup();

    is_initialized = true;
  }

  end:
  return is_initialized;
}
