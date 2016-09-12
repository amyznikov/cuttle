/*
 * cuttle/pg.c
 *
 *  Created on: Jan 17, 2016
 *      Author: amyznikov
 */

#define _GNU_SOURCE

#include "cuttle/pg.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <poll.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>


/** Default database login args */
static struct {
  char * host;    // database server host name
  char * port;    // database server port as string
  char * dbname;  // database name
  char * user;    // database user name
  char * psw;     // database password
  char * options; // pg options
  char * tty;     // tty
} cfg;

/** Bufer for last error message */
static __thread char errmsg[256];



/** debug messages */

#define LOG_ERRNO(fn) \
    snprintf(errmsg, sizeof(errmsg) - 1, "%s() fails: %d %s", fn, errno, strerror(errno))

#define LOG_PGRES(fn,rc) \
    snprintf(errmsg, sizeof(errmsg)-1, "%s() fails: %s/%s\n", \
        fn, PQresStatus(PQresultStatus(rc)),PQresultErrorMessage(rc))

#define LOG_PGCONN(pgc) \
    strncpy(errmsg, PQerrorMessage(pgc), sizeof(errmsg) - 1)



void pg_set_dblogin(const char * host, const char * port, const char * dbname, const char * user, const char * psw,
    const char * options, const char * tty)
{
  free(cfg.host), cfg.host = host ? strdup(host) : NULL;
  free(cfg.port), cfg.port = port ? strdup(port) : NULL;
  free(cfg.dbname), cfg.dbname = dbname ? strdup(dbname) : NULL;
  free(cfg.user), cfg.user = user ? strdup(user) : NULL;
  free(cfg.psw), cfg.psw = psw ? strdup(psw) : NULL;
  free(cfg.options), cfg.options = options ? strdup(options) : NULL;
  free(cfg.tty), cfg.tty = tty ? strdup(tty) : NULL;
}


const char * pg_errmsg(void)
{
  return errmsg;
}


PGconn * pg_login(const struct PGloginargs * args)
{
  const char * host, *port, *dbname, *user, *psw, *options, *tty;
  PGconn * pgc;

  if ( args ) {
    host = args->host;
    port = args->port;
    dbname = args->dbname;
    user = args->user;
    psw = args->psw;
    options = args->options;
    tty = args->tty;
  }
  else {
    host = cfg.host;
    port = cfg.port;
    dbname = cfg.dbname;
    user = cfg.user;
    psw = cfg.psw;
    options = cfg.options;
    tty = cfg.tty;
  }

  pgc = PQsetdbLogin(host, port, options, tty, dbname, user, psw);

  if ( PQstatus(pgc) != CONNECTION_OK ) {
    LOG_PGCONN(pgc);
    PQfinish(pgc);
    pgc = NULL;
  }

  return pgc;
}

void pg_close(PGconn ** pgc)
{
  if ( pgc && *pgc ) {
    PQfinish(*pgc);
    *pgc = NULL;
  }
}


static PGresult * db_execv(PGconn * pgc, const char * format, va_list arglist)
{
  char * query = NULL;
  PGresult * rc = NULL;

  if ( vasprintf(&query, format, arglist) > 0 ) {
    rc = PQexec(pgc, query);
  }

  free(query);

  return rc;
}


static int db_getvaluesv(const PGresult * rc, int tup_num, va_list arglist)
{
  const char * fmt;
  void * val;
  const char * s;
  int n = 0;

  while ( (fmt = va_arg(arglist, const char *)) && (val = va_arg(arglist, void*)) ) {

    if ( !(s = PQgetvalue(rc, tup_num, n)) ) {
      LOG_PGRES("PQgetvalue",rc);
      break;
    }

    if ( *s != 0 && sscanf(s, fmt, val) != 1 ) {
      LOG_ERRNO("sscanf");
      break;
    }

    ++n;
  }

  return n;
}




PGiterator * pg_iterator_open(PGconn * pgc, const char * format, ...)
{
  va_list arglist;
  PGiterator * ii = NULL;
  PGresult * rc = NULL;
  bool fok = false;

  if ( !(ii = calloc(1, sizeof(*ii))) ) {
    LOG_ERRNO("calloc");
    goto end;
  }

  va_start(arglist, format);
  rc = db_execv(pgc, format, arglist);
  va_end(arglist);
  if ( !rc ) {
    LOG_ERRNO("db_execv()");
    goto end;
  }

  if ( PQresultStatus(rc) != PGRES_TUPLES_OK ) {
    LOG_PGRES("db_exec", rc);
    goto end;
  }

  ii->rc = rc;
  ii->n_tuples = PQntuples(rc);
  ii->n_fields = PQnfields(rc);
  fok = true;

end : ;

  if ( !fok ) {
    PQclear(rc);
    free(ii);
    ii = NULL;
  }

  return ii;
}

int pg_iterator_next(PGiterator * curpos, ... )
{
  va_list arglist;
  int status = 0;

  if ( curpos->n < curpos->n_tuples ) {

    va_start(arglist, curpos);
    status = db_getvaluesv(curpos->rc, curpos->n, arglist);
    va_end(arglist);

    if ( status == curpos->n_fields ) {
      ++curpos->n;
    }
    else {
      status = -1;
    }
  }

  return status;
}


void pg_iterator_close(PGiterator ** ii)
{
  if ( ii && *ii ) {
    PQclear((*ii)->rc);
    free(*ii), *ii = NULL;
  }
}


PGresult * pg_exec(PGconn * pgc, const char * format, ...)
{
  va_list arglist;
  PGresult * rc = NULL;

  va_start(arglist, format);
  if ( !(rc = db_execv(pgc, format, arglist)) ) {
    LOG_ERRNO("db_execv()");
  }
  va_end(arglist);

  return rc;
}

bool pg_exec_command(PGconn * pgc, const char * format, ...)
{
  va_list arglist;
  PGresult * rc = NULL;
  bool fok = false;

  va_start(arglist, format);
  rc = db_execv(pgc, format, arglist);
  va_end(arglist);

  if ( !rc ) {
    LOG_ERRNO("db_execv");
  }
  else if ( PQresultStatus(rc) != PGRES_COMMAND_OK ) {
    LOG_PGRES("db_execv", rc);
  }
  else {
    fok = true;
  }

  PQclear(rc);

  return fok;
}

PGresult * pg_exec_query(PGconn * pgc, const char * format, ...)
{
  va_list arglist;
  PGresult * rc = NULL;

  va_start(arglist, format);
  rc = db_execv(pgc, format, arglist);
  va_end(arglist);

  if ( !rc ) {
    LOG_ERRNO("db_execv");
  }
  else if ( PQresultStatus(rc) != PGRES_TUPLES_OK ) {
    LOG_PGRES("db_execv", rc);
    PQclear(rc);
    rc = NULL;
  }

  return rc;
}

int pg_getvalues(const PGresult * rc, int tup_num, ...)
{
  va_list arglist;
  int n;

  *errmsg = 0;
  va_start(arglist, tup_num);
  n = db_getvaluesv(rc, tup_num, arglist);
  va_end(arglist);

  return n;
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


static int64_t db_gettime(void)
{
  struct timespec tm;
  clock_gettime(CLOCK_MONOTONIC, &tm);
  return ((int64_t) tm.tv_sec * 1000 + tm.tv_nsec / 1000000);
}

bool pg_listen_notify(PGconn * pgc, const char * event)
{
  return pg_exec_command(pgc, "LISTEN %s", event);
}

bool pg_unlisten_notify(PGconn * pgc, const char * event)
{
  return pg_exec_command(pgc, "UNLISTEN %s", event);
}

/* fixme: more flexibility need, to allow untegrate with gRPC and PCL */
PGnotify * pg_get_notify(PGconn * pgc, int tmo)
{
  PGnotify * notify = NULL;
  int64_t tc, te;

  te = tmo < 0 ? INT64_MAX : db_gettime() + tmo;
  errno = EAGAIN;

  while ( !(notify = PQnotifies(pgc)) &&(tc = db_gettime()) <= te ) {

    struct pollfd pollfd = {
        .fd = PQsocket(pgc),
        .events = POLLIN,
        .revents = 0
    };

    if ( poll(&pollfd, 1, (int) (te - tc)) < 0 ) {
      break;
    }

    if ( (pollfd.revents & POLLIN) ) {
      PQconsumeInput(pgc);
    }
    else if ( (pollfd.revents & (POLLERR | POLLHUP | POLLNVAL)) ) {
      errno = EINVAL;
      break;
    }
  }

  return notify;
}

