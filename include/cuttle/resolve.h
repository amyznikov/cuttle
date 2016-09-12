/*
 * cuttle/resolve.h
 *
 * CF interface to William Ahern asynchronous DNS resolver
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_resolve_h__
#define __cuttle_resolve_h__

#include <netdb.h>

#ifdef __cplusplus
extern "C" {
#endif


struct cf_dns_query;

int cf_resolve_submit(struct cf_dns_query ** restrict pq, const char * name, const struct addrinfo * restrict hints);
int cf_resolve_pollfd(const struct cf_dns_query * q);
int cf_resolve_fetch(const struct cf_dns_query * q, struct addrinfo ** ent);
void cf_resolve_destroy(struct cf_dns_query ** q);
const char * cf_resolve_strerror(int status);




#ifdef __cplusplus
}
#endif

#endif /* __cuttle_resolve_h__ */
