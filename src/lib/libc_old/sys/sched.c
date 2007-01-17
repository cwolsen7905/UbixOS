
#include <sys/sched.h>

int sched_yield(void)
{
	volatile int return_val = 0;
	asm volatile(
	"int %0\n"
	: : "i" (0x80),"a" (11)
	);
	return return_val;
}

