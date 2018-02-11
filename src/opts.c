/*
 * opts.c
 *
 *  Created on: Aug 27, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include "cuttle/opts.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>


bool cf_read_config_file(const char * fname, bool (*parseopt)(char * key, char * value))
{
  FILE * fp = NULL;
  char line[1024] = "";
  int line_index = 0;
  int fok = 0;

  if ( !(fp = fopen(fname, "r")) ) {
    fprintf(stderr, "FATAL: Can't open '%s': %s\n", fname, strerror(errno));
    goto end;
  }

  fok = 1;

  while ( fgets(line, sizeof(line), fp) ) {

    char keyname[128] = "", keyvalue[PATH_MAX] = "";

    ++line_index;

    if ( sscanf(line, " %255[A-Za-z1-9_:-.] = %255[^#\n]", keyname, keyvalue) >= 1 && *keyname != '#' ) {
      if ( !(fok = parseopt(keyname, keyvalue)) ) {
        fprintf(stderr, "Error in config file '%s' at line %d: '%s'\n", fname, line_index, line);
        break;
      }
    }
  }

end : ;

  if ( fp ) {
    fclose(fp);
  }

  return fok;
}

bool cf_parse_option(char * line, bool (*parseopt)(char * key, char * value))
{
  char keyname[128] = "", keyvalue[PATH_MAX] = "";
  bool fok = false;

  if ( sscanf(line, " %255[A-Za-z1-9_:-.] = %255[^#\n]", keyname, keyvalue) < 1 || *keyname == '#' ) {
    errno = ENOMSG;
  }
  else {
    errno = 0;
    fok = parseopt(keyname, keyvalue);
  }

  return fok;
}


const char * cf_find_config_file(const char * service_name, char config_file_name[PATH_MAX])
{
  struct passwd * pw;
  uid_t uid;


  snprintf(config_file_name, PATH_MAX - 1, "./%s", service_name);
  if ( access(config_file_name, F_OK) == 0 ) {
    goto end;
  }

  if ( (uid = geteuid()) != 0 && (pw = getpwuid(uid)) != NULL ) {
    snprintf(config_file_name, PATH_MAX - 1, "%s/.config/%s/%s", pw->pw_dir, service_name, service_name);
    if ( access(config_file_name, F_OK) == 0 ) {
      goto end;
    }
  }

  snprintf(config_file_name, PATH_MAX - 1, "/var/lib/%s/%s", service_name, service_name);
  if ( access(config_file_name, F_OK) == 0 ) {
    goto end;
  }

  snprintf(config_file_name, PATH_MAX - 1, "/usr/local/etc/%s/%s", service_name, service_name);
  if ( access(config_file_name, F_OK) == 0 ) {
    goto end;
  }

  snprintf(config_file_name, PATH_MAX - 1, "/etc/%s/%s", service_name, service_name);
  if ( access(config_file_name, F_OK) == 0 ) {
    goto end;
  }

  *config_file_name = 0;

end: ;

  return *config_file_name ? config_file_name : NULL;
}
