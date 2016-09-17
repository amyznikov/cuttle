/*
 * corpc/service.h
 *
 *  Created on: Sep 11, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef __cuttle_corpc_service_h__
#define __cuttle_corpc_service_h__

#ifdef __cplusplus
extern "C" {
#endif


struct corpc_stream;

typedef
struct corpc_service_method {
  const char * name;
  void (*proc)(struct corpc_stream * st);
} corpc_service_method;

typedef
struct corpc_service {
  const char * name;
  const corpc_service_method methods[];
} corpc_service;



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_corpc_service_h__ */
