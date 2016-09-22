/*
 * android-spin-lock.h
 *
 */

//#pragma once

#ifndef __cuttle_android_spin_lock_h__
#define __cuttle_android_spin_lock_h__


#ifdef __cplusplus
extern "C" {
#endif

typedef int pthread_spinlock_t;

#ifndef PTHREAD_PROCESS_SHARED
# define PTHREAD_PROCESS_SHARED 1
#endif
#ifndef PTHREAD_PROCESS_PRIVATE
# define PTHREAD_PROCESS_PRIVATE 2
#endif

static inline int pthread_spin_init(pthread_spinlock_t *lock, int pshared) {
  (void)(pshared);
  __asm__ __volatile__ ("" ::: "memory");
  *lock = 0;
  return 0;
}

static inline int pthread_spin_destroy(pthread_spinlock_t *lock) {
  (void)(lock);
  return 0;
}

static inline int pthread_spin_lock(pthread_spinlock_t *lock) {
  while (1) {
    for (int i=0; i < 10000; i++) {
      if (__sync_bool_compare_and_swap(lock, 0, 1)) {
        return 0;
      }
    }
    sched_yield();
  }
}

static inline int pthread_spin_trylock(pthread_spinlock_t *lock) {
  if (__sync_bool_compare_and_swap(lock, 0, 1)) {
    return 0;
  }
  return EBUSY;
}

static inline int pthread_spin_unlock(pthread_spinlock_t *lock) {
  __asm__ __volatile__ ("" ::: "memory");
  *lock = 0;
  return 0;
}



#ifdef __cplusplus
}
#endif

#endif /* __cuttle_android_spin_lock_h__ */
