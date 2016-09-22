/*
 * ssl-init.h
 *
 *  Created on: Aug 31, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_ssl_init_h__
#define __cuttle_ssl_init_h__

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


bool cf_ssl_initialize(const char * openssl_conf);

/**
 * Returns comiled OPENSSL_VERSION_NUMBER
 * */
int cf_get_opennssl_version_number(void);

/**
 * Returns comiled OPENSSL_VERSION_TEXT
 * */
const char * cf_get_opennssl_version_string(void);

/**
 * Calls SSLeay_version(SSLEAY_VERSION)
 * */
const char * cf_get_runtime_opennssl_version_string(void);

#ifdef __cplusplus
}
#endif

#endif /* __cuttle_ssl_init_h__ */
