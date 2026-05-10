/*
 * Minimal stub for UbixOS userland build.
 * Provides the pthread type definitions needed by FreeBSD-derived libc headers.
 */

#ifndef _SYS__PTHREADTYPES_H_
#define _SYS__PTHREADTYPES_H_

struct pthread;
struct pthread_mutex;
struct pthread_cond;
struct pthread_rwlock;
struct pthread_barrier;
struct pthread_once;

typedef struct pthread *pthread_t;
typedef struct pthread_mutex *pthread_mutex_t;
typedef struct pthread_cond  *pthread_cond_t;
typedef struct pthread_rwlock *pthread_rwlock_t;
typedef struct pthread_barrier *pthread_barrier_t;
typedef struct pthread_once pthread_once_t;

struct pthread_once {
  int state;
};

typedef int pthread_key_t;
typedef int pthread_mutexattr_t;
typedef int pthread_condattr_t;
typedef int pthread_rwlockattr_t;
typedef int pthread_barrierattr_t;
typedef int pthread_attr_t;

#endif /* _SYS__PTHREADTYPES_H_ */
