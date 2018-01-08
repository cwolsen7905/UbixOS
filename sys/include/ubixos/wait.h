#ifndef _UBIXOS_WAIT_H
#define _UBIXOS_WAIT_H

#include <ubixos/sched.h>

struct wait_queue {
  struct kTask_t *task;
  struct wait_queue *next;
};

struct semaphore {
  int sount;
  struct wait_queue *wait;
};

#endif
