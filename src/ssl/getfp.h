/*
 * getfp.h
 *
 *  Created on: Sep 1, 2016
 *      Author: amyznikov
 */

//#pragma once

#ifndef ___libcuttle_src_getfp_h___
#define ___libcuttle_src_getfp_h___

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif


FILE * cf_getfp(const char * fname, const char * mode, bool * fok);
void cf_closefp(FILE ** fp);


#ifdef __cplusplus
}
#endif

#endif /* ___libcuttle_src_getfp_h___ */
