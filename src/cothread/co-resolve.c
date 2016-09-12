/*
 * co-resolve.c
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

#include <cuttle/cothread/scheduler.h>
#include <cuttle/cothread/resolve.h>
#include <cuttle/dns/resolve.h>
#include <errno.h>


int co_resolve(const char * name, struct addrinfo ** restrict aip, const struct addrinfo * restrict hints,
    time_t timeout_sec)
{
  struct cf_dns_query * q = NULL;
  int status;
  int so;

  if ( timeout_sec > 0 ) {
    timeout_sec = time(NULL) + timeout_sec;
  }

  if ( (status = cf_resolve_submit(&q, name, hints)) ) {
    goto end;
  }

  errno = 0;

  while ( 42 ) {

    if ( (status = cf_resolve_fetch(q, aip)) == 0 ) {
      break;
    }

    if ( status == EAGAIN ) {

      if ( timeout_sec > 0 && time(NULL) > timeout_sec ) {
        errno = ETIMEDOUT;
        break;
      }

      if ( (so = cf_resolve_pollfd(q)) == -1 ) {
        break;
      }

      co_io_wait(so, EPOLLIN, 1000);
      continue;
    }

    break;
  }

end:;

  cf_resolve_destroy(&q);

  return status;
}


const char * co_resolve_strerror(int status)
{
  return cf_resolve_strerror(status);
}
