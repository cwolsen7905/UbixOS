/**
 * tasks.c
 *
 * ??
 *
 * Exports:
 *  task1()
 *  task2()
 *  task3()
 *  task4()
 *
 * Imports:
 *  video.c		putch_help();
 *  sched.c		task_t *_curr_task;
 */

#include "_krnl.h"

/**
 * Imports
 */
void putch_help(console_t *con, unsigned c);
extern task_t *_curr_task;
void schedule(void);

/**
 * write()
 *
 */
static int write(const unsigned char *str, unsigned len)
{
	unsigned i;

	for(i = 0; i < len; i++)
	{
		putch_help(_curr_task->vc, *str);
		str++;
	}
	return i;
}

/**
 * yield()
 *
 */
static void yield(void)
{
	schedule();
}

#define	WAIT	0xFFFFFL

/**
 * wait()
 */
static void wait(void)
{
	unsigned long wait;

	for(wait = WAIT; wait != 0; wait--)
		/* nothing */;
}

/**
 * task1()
 *
 */
void task1(void)
{
	//static const unsigned char msg_a[] = "root@hybos $ ";
/**/

	//write(msg_a, sizeof(msg_a));
	wait();
	while(1)
	{
		/* so we can process other events */
		yield();
		wait();
	}
}

/**
 * task2()
 *
 */
void task2(void)
{
	//static const unsigned char msg_a[] = "root@hybos $ ";
/**/

	//write(msg_a, sizeof(msg_a));
	wait();
	while(1)
	{
		yield();
		wait();
	}
}

/**
 * task3()
 *
 */
void task3(void)
{
	//static const unsigned char msg_a[] = "root@hybos $ ";
/**/

	//write(msg_a, sizeof(msg_a));
	wait();
	while(1)
	{
		yield();
		wait();
	}
}

/**
 * task4()
 *
 */
void task4(void)
{
	//static const unsigned char msg_a[] = "root@hybos $ ";
/**/

	//write(msg_a, sizeof(msg_a));
	wait();
	while(1)
	{
		yield();
		wait();
	}
}

