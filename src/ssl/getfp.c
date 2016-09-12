/*
 * getfp.c
 *
 *  Created on: Sep 1, 2016
 *      Author: amyznikov
 */

#include <cuttle/ssl/error.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include "getfp.h"


FILE * cf_getfp(const char * fname, const char * mode, bool * fok)
{
  FILE * fp = NULL;

  if ( !fname || !*fname ) {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "No file name provided");
  }
  else if ( *mode == 'r' ) {
    if ( strcasecmp(fname, "stdin") == 0 ) {
      fp = stdin;
    }
    else if ( !(fp = fopen(fname, mode)) ) {
      CF_SSL_ERR(CF_SSL_ERR_STDIO, "Can not open '%s': %s", fname, strerror(errno));
    }
  }
  else if ( *mode == 'w' || *mode == 'a' ) {
    if ( strcasecmp(fname, "stdout") == 0 ) {
      fp = stdout;
    }
    else if ( strcasecmp(fname, "stderr") == 0 ) {
      fp = stderr;
    }
    else if ( strcmp(fname, "/dev/null") == 0 ) {
      if ( fok ) {
        *fok = true;
      }
    }
    else if ( !(fp = fopen(fname, mode)) ) {
      CF_SSL_ERR(CF_SSL_ERR_STDIO, "Can not open '%s': %s", fname, strerror(errno));
    }
  }
  else {
    CF_SSL_ERR(CF_SSL_ERR_STDIO, "Invalid file mode specified %s", mode);
  }

  return fp;
}

void cf_closefp(FILE ** fp)
{
  if ( fp && *fp ) {
    if ( *fp != stdin && *fp != stdout && *fp != stderr ) {
      fclose(*fp);
    }
    *fp = NULL;
  }
}



