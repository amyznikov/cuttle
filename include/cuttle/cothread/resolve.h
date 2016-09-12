/*
 * cuttle/cothread/resolve.h
 *
 *  Created on: Sep 8, 2016
 *      Author: amyznikov
 */

// #pragma once

#ifndef __cuttle_co_resolve_h__
#define __cuttle_co_resolve_h__

#include <netdb.h>
#include <stdbool.h>
#include <time.h>
#include <cuttle/dns/resolve.h>

#ifdef __cplusplus
extern "C" {
#endif


int co_resolve(const char * name,
    struct addrinfo ** restrict aip,
    const struct addrinfo * restrict hints,
    time_t timeout_sec);

const char * co_resolve_strerror(int status);

#ifdef __cplusplus
}
#endif

#endif /* __cuttle_co_resolve_h__*/
