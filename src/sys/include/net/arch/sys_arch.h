#ifndef __ARCH_SYS_ARCH_H__
#define __ARCH_SYS_ARCH_H__

#include <ubixos/ubthread.h>

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL  NULL
#define SYS_MBOX_SIZE 100

struct sys_timeouts {
  struct sys_timeout *next;
};

struct sys_sem {
  unsigned int c;
  ubthread_cond_t cond;
  ubthread_mutex_t mutex;
};

typedef struct sys_sem * sys_sem_t;

struct sys_mbox {
  uInt16 first, last;
  void *msgs[SYS_MBOX_SIZE];
  struct sys_sem *mail;
  struct sys_sem *mutex;
};
typedef struct sys_mbox *sys_mbox_t;

struct sys_thread {
  struct sys_thread *next;
  struct sys_timeouts timeouts;
  kTask_t *ubthread;
  char name[128];
};

typedef struct sys_thread * sys_thread_t;


//void sys_thread_new(void (*)(void), void *);

#endif
