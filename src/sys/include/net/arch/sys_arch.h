#ifndef __ARCH_SYS_ARCH_H__
#define __ARCH_SYS_ARCH_H__

#define SYS_MBOX_NULL NULL
#define SYS_SEM_NULL  NULL

/*
struct sys_sem;
typedef struct sys_sem * sys_sem_t;

struct sys_mbox;
typedef struct sys_mbox *sys_mbox_t;

struct sys_thread;
typedef struct sys_thread * sys_thread_t;
*/

struct sys_timeouts {
  struct sys_timeout *next;
};

#endif
