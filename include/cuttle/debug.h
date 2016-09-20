/*
 * cuttle/debug.h
 *
 *  Created on: Aug 27, 2016
 *      Author: amyznikov
 */


#ifndef __cuttle_debug_h__
#define __cuttle_debug_h__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  CF_LOG_FATAL   = 0,   /* system is unusable */
  CF_LOG_CRITICAL= 1,   /* critical conditions */
  CF_LOG_ERROR   = 2,   /* error conditions */
  CF_LOG_WARNING = 3,   /* warning conditions */
  CF_LOG_NOTICE  = 4,   /* normal but significant condition */
  CF_LOG_INFO    = 5,   /* informational */
  CF_LOG_DEBUG   = 6,   /* debug-level messages */
  CF_LOG_EVENT   = 0x8  /* custom event masks start here */
};


bool cf_set_logfilename(const char * fname);
const char * cf_get_logfilename(void);

void cf_set_loglevel(uint32_t mask);
uint32_t cf_get_loglevel(void);

bool cf_setup_signal_handler(void);


void cf_plogv(int pri, const char * func, int line, const char * format, va_list arglist);
void cf_plog(int pri, const char * func, int line, const char * format, ...)
  __attribute__ ((__format__ (__printf__, 4, 5)));



void cf_pbt(void);

#define CF_FATAL(...)     cf_plog(CF_LOG_FATAL  , __func__, __LINE__, __VA_ARGS__)
#define CF_CRITICAL(...)  cf_plog(CF_LOG_CRITICAL,__func__, __LINE__, __VA_ARGS__)
#define CF_ERROR(...)     cf_plog(CF_LOG_ERROR  , __func__, __LINE__, __VA_ARGS__)
#define CF_WARNING(...)   cf_plog(CF_LOG_WARNING, __func__, __LINE__, __VA_ARGS__)
#define CF_NOTICE(...)    cf_plog(CF_LOG_NOTICE , __func__, __LINE__, __VA_ARGS__)
#define CF_INFO(...)      cf_plog(CF_LOG_INFO   , __func__, __LINE__, __VA_ARGS__)
#define CF_DEBUG(...)     cf_plog(CF_LOG_DEBUG  , __func__, __LINE__, __VA_ARGS__)
#define CF_EVENT(e,...)   cf_plog(e, __func__, __LINE__, __VA_ARGS__)

// fixme: check the http://svn.pld-linux.org/svn/backtracexx/
#define CF_PBT()          cf_pbt()

#ifdef __cplusplus
}
#endif

#endif /* __cuttle_debug_h__ */
