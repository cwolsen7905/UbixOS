#ifndef _UBIXOS_WAIT_H
#define _UBIXOS_WAIT_H


struct kTask_t;

struct wait_queue {
  struct kTask_t *task;
  struct wait_queue *next;
};

struct semaphore {
  int sount;
  struct wait_queue *wait;
};

#define sti() __asm__ __volatile__ ("sti": : :"memory")
#define cli() __asm__ __volatile__ ("cli": : :"memory")
#define nop() __asm__ __volatile__ ("nop")

#define save_flags(x) __asm__ __volatile__("pushfl ; popl %0":"=r" (x): /* no input */ :"memory")

#define restore_flags(x) __asm__ __volatile__("pushl %0 ; popfl": /* no output */ :"r" (x):"memory")

#endif
