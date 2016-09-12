/*
 * time.c
 *
 *  Created on: Sep 9, 2016
 *      Author: amyznikov
 */

#include <cuttle/time.h>


int64_t cf_get_realtime_ms(void)
{
  struct timespec tm = { .tv_sec = 0, .tv_nsec = 0 };
  clock_gettime(CLOCK_MONOTONIC, &tm);
  return ((int64_t) tm.tv_sec * 1000 + (int64_t) (tm.tv_nsec / 1000000));
}

int64_t cf_get_realtime_us(void)
{
  struct timespec tm = { .tv_sec = 0, .tv_nsec = 0 };
  clock_gettime(CLOCK_MONOTONIC, &tm);
  return ((int64_t) tm.tv_sec * 1000000 + (int64_t) (tm.tv_nsec / 1000));
}

int64_t cf_get_monotic_ms(void)
{
  struct timespec tm = { .tv_sec = 0, .tv_nsec = 0 };
  clock_gettime(CLOCK_REALTIME, &tm);
  return ((int64_t) tm.tv_sec * 1000 + (int64_t) (tm.tv_nsec / 1000000));
}

int64_t cf_get_monotic_us(void)
{
  struct timespec tm = { .tv_sec = 0, .tv_nsec = 0 };
  clock_gettime(CLOCK_REALTIME, &tm);
  return ((int64_t) tm.tv_sec * 1000000 + (int64_t) (tm.tv_nsec / 1000));
}
