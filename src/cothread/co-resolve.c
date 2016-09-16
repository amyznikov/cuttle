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


bool co_server_resolve(struct addrinfo ** ai, const char * address, uint16_t port, int tmo)
{
  bool fok = false;

  const struct addrinfo addrshints = {
    .ai_family = PF_INET,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_V4MAPPED
  };

  if ( co_resolve(address, ai, &addrshints, tmo > 0 ? tmo / 1000 : 15 * 1000) != 0 ) {
    goto end;
  }

  if ( !port && (*ai)->ai_addr->sa_family != AF_UNIX ) {
    errno = EDESTADDRREQ;
    goto end;
  }

  fok = true;

  if ( (*ai)->ai_addr->sa_family == AF_INET ) {
    ((struct sockaddr_in*) (*ai)->ai_addr)->sin_port = htons(port);
  }
  else if ( (*ai)->ai_addr->sa_family == AF_INET6 ) {
    ((struct sockaddr_in6*) (*ai)->ai_addr)->sin6_port = htons(port);
  }

end:

  return fok;
}




const char * co_resolve_strerror(int status)
{
  return cf_resolve_strerror(status);
}
