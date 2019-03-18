/**
 * schedule.c
 *
 * Task creation and scheduling
 *
 * Exports:
 *  task_t *_current_task;
 *  schedule();
 *  init_tasks();
 *
 * Imports:
 *  main.c		printf();
 *  video.c		console_t _vc[];
 */

#include <setjmp.h> /* setjmp(), longjmp() */
#include <string.h>
#include "_krnl.h" /* console_t */
#include "bootlog.h" /* klog() */

/**
 * Imports
 */
void printf(const char *fmt, ...);
extern console_t _vc[];
void task1(void);
void task2(void);
void task3(void);
void task4(void);

#define	MAX_TASK	16
#define	USER_STACK_SIZE	512

/**
 * jmp_buf (E)IP and (E)SP register names for various environments
 */

/**
 * Tinylib (default)
 * These should work for most compilers - The HybOS
 * compiler (modified gcc) uses this as the default.
 * Your mileage may vary.
 */
#define	JMPBUF_IP		eip
#define	JMPBUF_SP		esp
#define	JMPBUF_FLAGS	eflags

#if 0
/**
 * TurboC
 *
 * These should work with all versions of TurboC's
 * compiler.
 */
#define	JMPBUF_IP		j_ip
#define	JMPBUF_SP		j_sp
#define	JMPBUF_FLAGS	j_flag

/**
 * DJGPP
 *
 * These should work with the DJGPP compiler
 */
#define	JMPBUF_IP		__eip
#define	JMPBUF_SP		__esp
#define	JMPBUF_FLAGS	__eflags

#define	JMPBUF_IP		__pc
#define	JMPBUF_SP		__sp
#define	JMPBUF_FLAGS	????????

/**
 * glibc5
 *
 * I have no idea what the register name is
 * for JMPBUF_FLAGS. Good luck.
 */
#define	JMPBUF_IP		eip
#define	JMPBUF_SP		esp
#define	JMPBUF_FLAGS	eflags
#endif /* 0 */

task_t *_curr_task;
static task_t _tasks[MAX_TASK];

/**
 * schedule()
 *
 */
void schedule(void)
{
	static unsigned current;

	/**
	 * If setjmp() returns non-zero it means that we came here through
	 * hyperspace from our call to longjmp() below, so just return
	 */
/** UBU
	if(setjmp(_curr_task->state) != 0)
		return;
**/

	/**
	 * Try to find the next runnable task
	 */
	do
	{
		current++;
		if(current >= MAX_TASK)
			current = 0;
		_curr_task = _tasks + current;
	} while(_curr_task->status != TS_RUNNABLE);

	/**
	 * Jump to the new task
	 */
	longjmp(_curr_task->state, 1);
}
/*****************************************************************************
*****************************************************************************/
#define	NUM_TASKS	0

/**
 * init_tasks()
 *
 */
void init_tasks(void)
{
	static unsigned char stacks[NUM_TASKS][USER_STACK_SIZE];
	/*static unsigned entry[NUM_TASKS] =
	{
		0,			(unsigned)task1,
		(unsigned)task2,	(unsigned)task3,
		(unsigned)task4
	};*/
	static unsigned entry[NUM_TASKS];

	unsigned adr, i;

	klog("init", "task handler", K_KLOG_PENDING, &_vc[0]);

	/**
	 * For user taskes, initialize the saved state
	 */
	for(i = 1; i < NUM_TASKS; i++)
	{
		(void)setjmp(_tasks[i].state);

		/**
		 * especially the stack pointer
		 */
		adr = (unsigned)(stacks[i] + USER_STACK_SIZE);
		_tasks[i].state[0].JMPBUF_SP = adr;

		/**
		 * and program counter
		 */
		_tasks[i].state[0].JMPBUF_IP = entry[i];
		
		/**
		 * enable interrupts (by setting EFLAGS value)
		 */
		_tasks[i].state[0].JMPBUF_FLAGS = 0x200;

		/**
		 * allocate a virtual console to this task
		 */
		_tasks[i].vc = _vc + i;

		/**
		 * and mark it as runnable
		 */
		_tasks[i].status = TS_RUNNABLE;
	}

	/**
	 * mark task 0 runnable (idle task)
	 */
	_tasks[0].status = TS_RUNNABLE;

	/**
	 * set _curr_task so schedule() will save state
	 * of task 0
	 */
	_curr_task = _tasks + 0;

	klog(NULL, NULL, K_KLOG_SUCCESS, &_vc[0]);
}

