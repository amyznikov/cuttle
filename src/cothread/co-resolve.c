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
#include <arpa/inet.h>
#include <stdio.h>
#include <malloc.h>
#include <cuttle/debug.h>

#define INET_ADDR(a,b,c,d)      (uint32_t)((((uint32_t)(a))<<24)|((b)<<16)|((c)<<8)|(d))
#define INET_BYTE(n,x)          ((uint8_t)((x >> (n*8) ) & 0x000000FF))
#define IP4_ADDRSTRLEN          32


static bool parse_ip4_addrs(const char * addrs, uint32_t * address, uint16_t * port)
{
  uint8_t a1, a2, a3, a4;
  if ( sscanf(addrs, "%hhu.%hhu.%hhu.%hhu:%hu", &a1, &a2, &a3, &a4, port) >= 4 ) {
    *address = INET_ADDR(a1, a2, a3, a4);
    return true;
  }
  return false;
}


int co_resolve(const char * name, struct addrinfo ** restrict aip, const struct addrinfo * restrict hints,
    time_t timeout_sec)
{
  struct cf_dns_query * q = NULL;
  int status;
  int so;

  uint32_t address = 0;
  uint16_t port = 0;
  struct sockaddr_in * sin;

  if ( parse_ip4_addrs(name, &address, &port) ) {
    *aip = calloc(1, sizeof(struct addrinfo));
    (*aip)->ai_addrlen = sizeof(struct sockaddr_in);
    (*aip)->ai_addr = calloc(1, sizeof(struct sockaddr_in));
    sin = (struct sockaddr_in *)(*aip)->ai_addr;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    sin->sin_addr.s_addr = htonl(address);
    status = 0;
    goto end;
  }



  if ( timeout_sec > 0 ) {
    timeout_sec = time(NULL) + timeout_sec;
  }

  CF_DEBUG("cf_resolve_submit(%s)", name);
  if ( (status = cf_resolve_submit(&q, name, hints)) ) {
    CF_CRITICAL("cf_resolve_submit(%s) fails", name);
    goto end;
  }

  errno = 0;

  while ( 42 ) {

    if ( (status = cf_resolve_fetch(q, aip)) == 0 ) {

        CF_DEBUG("*aip=%p", (*aip));
        if ( (*aip) ) {

          struct addrinfo * ai = *aip;
          while ( ai ) {
            CF_DEBUG("ai_canonname=%s", ai->ai_canonname);
            CF_DEBUG("ai_family=%d", ai->ai_family);
            CF_DEBUG("ai_addr=%p", ai->ai_addr);
            CF_DEBUG("ai_addrlen=%u", ai->ai_addrlen);

            if ( ai->ai_addr ) {
              CF_DEBUG("*ai_addr=%s", inet_ntoa(((struct sockaddr_in* ) (ai->ai_addr))->sin_addr));
            }

            ai = ai->ai_next;
          }
        }

        break;
    }

    if ( status == EAGAIN ) {

      if ( timeout_sec > 0 && time(NULL) > timeout_sec ) {
        errno = ETIMEDOUT;
        break;
      }

      if ( (so = cf_resolve_pollfd(q)) == -1 ) {
        CF_DEBUG("cf_resolve_pollfd() fails");
        break;
      }

      CF_DEBUG("C co_io_wait");
      co_io_wait(so, EPOLLIN, 1000);
      CF_DEBUG("R co_io_wait");
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
    .ai_family = PF_UNSPEC,
    .ai_socktype = SOCK_STREAM,
    .ai_flags = AI_V4MAPPED
  }; //

  CF_DEBUG("co_resolve(%s:%u)", address, port);

  if ( co_resolve(address, ai, &addrshints, tmo > 0 ? tmo / 1000 : 15 * 1000) != 0 ) {
    CF_CRITICAL("co_resolve(%s:%u) fails", address, port);
    goto end;
  }

  CF_DEBUG("%s:%u : (*ai)->ai_addr->sa_family=%d", address, port, (*ai)->ai_addr->sa_family);

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
