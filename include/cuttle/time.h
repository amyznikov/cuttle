/*
 * time.h
 *
 *  Created on: Sep 9, 2016
 *      Author: amyznikov
 */

#pragma once

#ifndef __cuttle_time_h__
#define __cuttle_time_h__

#include <time.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t cf_get_realtime_ms(void);
int64_t cf_get_realtime_us(void);
int64_t cf_get_monotic_ms(void);
int64_t cf_get_monotic_us(void);

#ifdef __cplusplus
}
#endif

#endif /* __cuttle_time_h__ */
