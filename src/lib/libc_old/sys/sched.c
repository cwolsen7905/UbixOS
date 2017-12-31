
#include <sys/sched.h>

//int sched_yield(void);

asm(
  ".text             \n"
  ".type sched_yield,@function\n"
  ".globl sched_yield\n"
  "sched_yield:      \n"
  "mov $331,%eax     \n"
  "int $0x80         \n"
  "ret               \n"
);

/*
{
  volatile int return_val = 0;
  asm volatile(
    "int %0\n"
    : : "i" (0x80),"a" (11)
    );
  return return_val;
}

*/
