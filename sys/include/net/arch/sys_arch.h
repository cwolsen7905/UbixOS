#ifndef __ARCH_SYS_ARCH_H__
#define __ARCH_SYS_ARCH_H__

#include <ubixos/ubthread.h>

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL  NULL
#define SYS_MBOX_SIZE 100

/* Structs */
struct sys_timeouts {
  struct sys_timeout *next;
};

struct sys_mutex {
  ubthread_mutex_t mutex;
};

typedef struct sys_mutex sys_mutex_t;

struct sys_mbox {
  uint32_t head;
  uint32_t tail;

  //MrOlsen (2017-12-28) - This will break because size is passable
  void *msgs[SYS_MBOX_SIZE];

  //struct ubthread_mutex *lock;

  struct sys_sem *empty;
  struct sys_sem *full;
  struct sys_sem *lock;

  int wait_send;

  //void **queue;
  //uint32_t size;
};

typedef struct sys_mbox sys_mbox_t;

struct sys_thread {
  struct sys_thread *next;
  struct sys_timeouts timeouts;
  kTask_t *ubthread;
  char name[128];
};

typedef struct sys_thread * sys_thread_t;


#endif
