/*
 * opts.h
 *
 *  Created on: Aug 27, 2016
 *      Author: amyznikov
 */


#ifndef __cuttle_opts_h__
#define __cuttle_opts_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

const char * cf_find_config_file(const char * service_name);
bool cf_read_config_file(const char * fname, bool (*parseopt)(char * key, char * value));
bool cf_parse_option(char * line, bool (*parseopt)(char * key, char * value));




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_opts_h__ */
