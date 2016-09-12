/*
 * pgdb.h
 *
 *  Created on: Jan 17, 2016
 *      Author: amyznikov
 */


#ifndef __cuttle_pg_h__
#define __cuttle_pg_h__

#include "libpq-fe.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** PG Login Args */
typedef
struct PGloginargs {
  const char * host;
  const char * port;
  const char * dbname;
  const char * user;
  const char * psw;
  const char * options;
  const char * tty;
} PGloginargs;


/** Query result iterator for requests returning multiple rows
 * */
typedef
struct PGiterator {
  PGresult * rc;
  int n, n_tuples, n_fields;
} PGiterator;



/**
 *  get last error message
 */
const char * pg_errmsg(void);



/**
 *  Sets default login args
 */
void pg_set_dblogin(const char * host,
    const char * port,
    const char * dbname,
    const char * user,
    const char * psw,
    const char * options,
    const char * tty);



/**
 *  Setup new database connection
 */
PGconn * pg_login(const struct PGloginargs * args);


/**
 *  Close database connection and free the PGconn instance.
 */
void pg_close(PGconn ** dbc);



/**
 *  Execute an SQL command or query on database
 *    rc = pg_exec(dbc, "select * from users where uid=%d", uid);
 */
PGresult * pg_exec(PGconn * dbc, const char * format, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));



/**
 *  Execute an SQL command not returning result
 *    if ( !pg_exec_command(dbc, "delete from users where uid=%d", uid) ) {
 *     ...
 *    }
 */
bool pg_exec_command(PGconn * dbc, const char * format, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));


/**
 *  Execute an SQL query returning set of tuples
 *    if ( !(rc = pg_exec_query(dbc, "select * from users where uid=%d", uid)) ) {
 *     ...
 *    }
 */
PGresult * pg_exec_query(PGconn * dbc, const char * format, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));


/**
 *  Extact values from tuple
 *    n = pg_getvalues(rc, tup_num, "%d", &x, "%f", &y, NULL);
 */
int pg_getvalues(const PGresult * rc, int tup_num, ...);


/**
 *  Execute SQL query and return iterator to iterate result rows
 *    it = pg_iterator_open(dbc, "select * from users where name like '%s' ", name);
 */
PGiterator * pg_iterator_open(PGconn * dbc, const char * format, ...)
  __attribute__ ((__format__ (__printf__, 2, 3)));


/**
 *  Close database iterator.
 */
void pg_iterator_close(PGiterator ** it);


/**
 *  Return: >0 on success, 0 on end of data, -1 on error
 */
int pg_iterator_next(PGiterator * curpos, ... );


/**
 *
 */
bool pg_listen_notify(PGconn * dbc, const char * event);
bool pg_unlisten_notify(PGconn * dbc, const char * event);
PGnotify * pg_get_notify(PGconn * dbc, int tmo);




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_pg_h__ */
