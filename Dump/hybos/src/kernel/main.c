/**
 * main.c
 *
 * Main code for HybOS.
 *
 * I spent a lot of time cleaning this damned thing up, so if you
 * are even REMOTELY thinking of modifying it, you better make
 * sure you follow the same design pattern as you see now. I am sick
 * of cleaning up c-style comments because some dumbass is too fucking
 * lazy to use the PROPER c89-style comments. This is C people, not C++.
 *
 * Exports:
 * 	void printf(const char *fmt, ...);
 * 	int main(void);
 *
 * Imports:
 * 	kstart.asm	getvect();
 * 	kstart.asm	setvect();
 * 	video.c		console_t _vc[];
 * 	video.c		blink();
 * 	video.c		init_video();
 * 	kbd.c			keyboard_irq();
 * 	kbd.c			kbd_hw_init();
 * 	kbd.c			init_keyboard();
 * 	sched.c		schedule();
 * 	sched.c		init_tasks();
 * 	debug.c		dump_regs();
 *
 * FIXME:
 * 	needs to be renamed to kernel.c
 */
#include <stdarg.h> /* va_list, va_start(), va_end() */
/*#include <string.h>*/ /* NULL */
#include <stdio.h> /* NULL */
#include <x86.h> /* disable() */
#include <_printf.h> /* do_printf() */
#include <_malloc.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include "_krnl.h" /* regs_t */
#include "bootlog.h" /* klog() */

/**
 * FIXME
 *
 * These externs and declares are a fucking mess and
 * need to be ported to their own headers for portability
 */

/**
 * Imports
 */
void getvect(vector_t *v, unsigned vect_num);
void setvect(vector_t *v, unsigned vect_num);
extern console_t _vc[];
void blink(void);
void putch(unsigned c);
void init_video(void);
void keyboard_irq(void);
//void kbd_hw_int(void);
void init_keyboard(void);
void schedule(void);
void init_tasks(void);
void dump_regs(regs_t *regs);

void _mm_physical_init(void);
unsigned _mm_physical_alloc(void);
void _mm_physical_free(unsigned page);
void _mm_page_copy_byte(uint32_t dest, uint32_t src);
void _mm_page_copy_word(uint32_t dest, uint32_t src);
void _mm_page_copy_dword(uint32_t dest, uint32_t src);

/*void init_cpu(void);*/

/**
 * printf/kprintf helper
 */
static int kprintf_help(unsigned c, void **ptr)
{
	/**
	 * Leave this for now
	 */
	ptr = ptr;

	putch(c);
	return 0;
}

/**
 * Format output and print it to stdout (vtty0)
 * Just like on any other operating system
 */
/*void printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void)do_printf(fmt, args, kprintf_help, NULL);
	va_end(args);
}*/

void kprintf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void)do_printf(fmt, args, kprintf_help, NULL);
	va_end(args);
}

/**
 * Format output and print it to stdout (vtty0)
 * Just like on any other operating system
 */
void printk(const char *fmt, ...)
{
	va_list args;

	/**
	 * TODO
	 *
	 * Select vtty0
	 */
	va_start(args, fmt);
	(void)do_printf(fmt, args, kprintf_help, NULL);
	va_end(args);
}

/**
 * Oh yeah, the fun function ;)
 */
void panic(const char *fmt, ...)
{
	va_list args;
	
	disable(); /* interrupts off */
	va_start(args, fmt);
	_vc[0].attrib = 15;
	printf("\n\npanic: ");	
	(void)do_printf(fmt, args, kprintf_help, NULL);

	printf("\n\nSystem halted.");
	__asm__ __volatile__ ("hlt");

	while(1)
		/* freeze */;
}

/**
 * Called when a kernel fault is detected. This does not
 * (normally) get called when something goes awry in
 * user-space, therefore it is designed for kernel-space
 */
void fault(regs_t *regs)
{
	struct exception
	{
		char *message;
		int signal;
		int processor;
	};
	
	static const struct exception ex[] =
	{
		{"Divide error", SIGFPE, 86},
		{"Debug exception", SIGTRAP, 86},
		{"Nonmaskable interrupt (NMI)", SIGBUS, 86},
		{"Breakpoint (INT3)", SIGEMT, 86},
		{"Overflow (INTO)", SIGFPE, 186},
		{"Bounds check", SIGFPE, 186},
		{"Invalid opcode", SIGILL, 186},
		{"Coprocessor not available", SIGFPE, 186},
		{"Double fault", SIGBUS, 286},
		{"Coprocessor segment overrun", SIGSEGV, 286},
		{"Invalid TSS", SIGSEGV, 286},
		{"Segment not present", SIGSEGV, 286},
		{"Stack exception", SIGSEGV, 286},
		{"General Protection Fault", SIGSEGV, 286},
		{"Page fault", SIGSEGV, 386},
		{NULL, SIGILL, 0},
		{"Coprocessor error", SIGFPE, 386},
		{"Alignment check",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"??",0,0},
		{"IRQ0",0,0},
		{"IRQ1",0,0},
		{"IRQ2",0,0},
		{"IRQ3",0,0},
		{"IRQ4",0,0},
		{"IRQ5",0,0},
		{"IRQ6",0,0},
		{"IRQ7",0,0},
		{"IRQ8",0,0},
		{"IRQ9",0,0},
		{"IRQ10",0,0},
		{"IRQ11",0,0},
		{"IRQ12",0,0},
		{"IRQ13",0,0},
		{"IRQ14",0,0},
		{"IRQ15",0,0},
		{"syscall",0,0}
	};


	switch(regs->which_int)
	{
		/**
		 * this handler installed at compile-time
		 * Keyboard handler is installed at run-time (see below)
		 */
		case 0x20:	/* timer IRQ 0 */
			//blink();
			/**
			 * reset hardware interrupt at 8259 chip
			 */
			outportb(0x20, 0x20);
		break;
		default:
			_vc[0].attrib = 15;
			printf("\n\npanic: Exception 0x%08X", regs->which_int);
			if(regs->which_int <= sizeof(ex) / sizeof(ex[0].message))
				printf(" (%s)", ex[regs->which_int].message);
			printf("\n");
			dump_regs(regs);
			printf("\n\nSystem halted.");
			__asm__ __volatile__ ("hlt");
		break;
	}
}

/**
 * ??
 */
static void init_8259s(void)
{
	static const unsigned irq0_int = 0x20, irq8_int = 0x28;

	/**
	 * Initialization Control Word #1 (ICW1)
	 */
	outportb(0x20, 0x11);
	outportb(0xA0, 0x11);

	/**
	 * ICW2:
	 * route IRQs 0-7 to INTs 20h-27h
	 */
	outportb(0x21, irq0_int);

	/**
	 * route IRQs 8-15 to INTs 28h-2Fh
	 */
	outportb(0xA1, irq8_int);

	/**
	 * ICW3
	 */
	outportb(0x21, 0x04);
	outportb(0xA1, 0x02);

	/**
	 * ICW4
	 */
	outportb(0x21, 0x01);
	outportb(0xA1, 0x01);

	/**
	 * enable IRQ0 (timer) and IRQ1 (keyboard)
	 */
	outportb(0x21, ~0x03);
	outportb(0xA1, ~0x00);
}

/**
 * MinGW32
 */
#ifdef __WIN32__
#if __GNUC__<3
#error Do not use MinGW GCC 2.x with NASM
#endif
	int __main(void) { return 0; }
	void _alloca(void) { }
#endif

/**
 * malloc, realloc, free, etc
 */
static char *g_heap_bot, *g_kbrk, *g_heap_top;
static void dump_heap(void)
{
	unsigned blks_used = 0, blks_free = 0;
	size_t bytes_used = 0, bytes_free = 0;
	malloc_t *m;
	int total;

	kprintf("===============================================\n");
	for(m = (malloc_t *)g_heap_bot; m != NULL; m = m->next)
	{
		printk("block %5p: %6u bytes %s\n", m,
			m->size, m->used ? "used" : "free");
		if(m->used)
		{
			blks_used++;
			bytes_used += m->size;
		}
		else
		{
			blks_free++;
			bytes_free += m->size;
		}
	}
	kprintf("blocks: %6u used, %6u free, %6u total\n", blks_used,
		blks_free, blks_used + blks_free);
	kprintf(" bytes: %6u used, %6u free, %6u total\n", bytes_used,
		bytes_free, bytes_used + bytes_free);
	kprintf("g_heap_bot=0x%p, g_kbrk=0x%p, g_heap_top=0x%p\n",
		g_heap_bot, g_kbrk, g_heap_top);
	total = (bytes_used + bytes_free) +
			(blks_used + blks_free) * sizeof(malloc_t);
	if(total != g_kbrk - g_heap_bot)
		kprintf("*** some heap memory is not accounted for\n");
	kprintf("===============================================\n");
}

void dumpheapk(void)
{
	dump_heap();
}

/**
 * POSIX sbrk() looks like this
 * void *sbrk(int incr);
 *
 * Mine is a bit different so I can signal the calling function
 * if more memory than desired was allocated (e.g. in a system with paging)
 * If your kbrk()/sbrk() always allocates the amount of memory you ask for,
 * this code can be easily changed.
 * 
 * int brk(	void *sbrk(		void *kbrk(
 * function		 void *adr);	 int delta);		 int *delta);
 * ----------------------	------------	------------		-------------
 * POSIX?			yes		yes			NO
 * return value if error	-1		-1			NULL
 * get break value		.		sbrk(0)			int x=0; kbrk(&x);
 * set break value to X	brk(X)		sbrk(X - sbrk(0))	int x=X, y=0; kbrk(&x) - kbrk(&y);
 * enlarge heap by N bytes	.		sbrk(+N)		int x=N; kbrk(&x);
 * shrink heap by N bytes	.		sbrk(-N)		int x=-N; kbrk(&x);
 * can you tell if you're
 * given more memory
 * than you wanted?	no		no			yes
 */
static void *kbrk(int *delta)
{
	static char heap[HEAP_SIZE];
	char *new_brk, *old_brk;

	/**
	 * heap doesn't exist yet
	 */
	if(g_heap_bot == NULL)
	{
		g_heap_bot = g_kbrk = heap;
		g_heap_top = g_heap_bot + HEAP_SIZE;
	}
	new_brk = g_kbrk + (*delta);

	/**
	 * too low: return NULL
	 */
	if(new_brk < g_heap_bot)
		return NULL;
	
	/**
	 * too high: return NULL
	 */
	if(new_brk >= g_heap_top)
		return NULL;

	/**
	 * success: adjust brk value...
	 */
	old_brk = g_kbrk;
	g_kbrk = new_brk;

	/**
	 * ...return actual delta... (for this sbrk(), they are the same)
	 * (*delta) = (*delta);
	 * ...return old brk value
	 */
	return old_brk;
}

/**
 * malloc() and free() use g_heap_bot, but not g_kbrk nor g_heap_top
 */
void *kmalloc(size_t size)
{
	unsigned total_size;
	malloc_t *m, *n;
	int delta;

	if(size == 0)
		return NULL;
	total_size = size + sizeof(malloc_t);

	/**
	 * search heap for free block (FIRST FIT)
	 */
	m = (malloc_t *)g_heap_bot;

	/**
	 * g_heap_bot == 0 == NULL if heap does not yet exist
	 */
	if(m != NULL)
	{
		if(m->magic != MALLOC_MAGIC)
		{
			/*printf("*** kernel heap is corrupt in kmalloc()\n");*/
			panic("kernel heap is corrupt in malloc()");
			return NULL;
		}
		for(; m->next != NULL; m = m->next)
		{
			if(m->used)
				continue;
			
			/**
			 * size == m->size is a perfect fit
			 */
			if(size == m->size)
				m->used = 1;
			else
			{
				/**
				 * otherwise, we need an extra sizeof(malloc_t) bytes for the header
				 * of a second, free block
				 */
				if(total_size > m->size)
					continue;

				/**
				 * create a new, smaller free block after this one
				 */
				n = (malloc_t *)((char *)m + total_size);
				n->size = m->size - total_size;
				n->next = m->next;
				n->magic = MALLOC_MAGIC;
				n->used = 0;
				
				/**
				 * reduce the size of this block and mark it used
				 */
				m->size = size;
				m->next = n;
				m->used = 1;
			}
			return (char *)m + sizeof(malloc_t);
		}
	}

	/**
	 * use kbrk() to enlarge (or create!) heap
	 */
	delta = total_size;
	n = kbrk(&delta);

	/**
	 * uh-oh
	 */
	if(n == NULL)
		return NULL;
	
	if(m != NULL)
		m->next = n;
	
	n->size = size;
	n->magic = MALLOC_MAGIC;
	n->used = 1;

	/**
	 * did kbrk() return the exact amount of memory we wanted?
	 * cast to make "gcc -Wall -W ..." shut the hell up
	 */
	if((int)total_size == delta)
		n->next = NULL;
	else
	{
		
		/**
		 * it returned more than we wanted (it will never return less):
		 * create a new, free block
		 */
		m = (malloc_t *)((char *)n + total_size);
		m->size = delta - total_size - sizeof(malloc_t);
		m->next = NULL;
		m->magic = MALLOC_MAGIC;
		m->used = 0;

		n->next = m;
	}
	return (char *)n + sizeof(malloc_t);
}

void kfree(void *blk)
{
	malloc_t *m, *n;

	/**
	 * get address of header
	 */
	m = (malloc_t *)((char *)blk - sizeof(malloc_t));
	if(m->magic != MALLOC_MAGIC)
	{
		/*printf("*** attempt to kfree() block at 0x%p with bad magic value\n", blk);*/
		panic("attempt to free() block at 0x%p with bad magic value", blk);
		return;
	}
	
	/**
	 * find this block in the heap
	 */
	n = (malloc_t *)g_heap_bot;
	if(n->magic != MALLOC_MAGIC)
	{
		/*printf("*** kernel heap is corrupt in kfree()\n");*/
		panic("kernel heap is corrupt in free()");
		return;
	}
	for(; n != NULL; n = n->next)
	{
		if(n == m)
			break;
	}
	
	/**
	 * not found? bad pointer or no heap or something else?
	 */
	if(n == NULL)
	{
		/*printf("*** attempt to kfree() block at 0x%p that is not in the heap\n", blk);*/
		panic("attempt to free() block at 0x%p that is not in the heap", blk);
		return;
	}

	/**
	 * free the block
	 */
	m->used = 0;

	/**
	 * coalesce adjacent free blocks
	 * Hard to spell, hard to do
	 */
	for(m = (malloc_t *)g_heap_bot; m != NULL; m = m->next)
	{
		while(!m->used && m->next != NULL && !m->next->used)
		{
			/**
			 * resize this block
			 */
			m->size += sizeof(malloc_t) + m->next->size;

			/**
			 * merge with next block
			 */
			m->next = m->next->next;
		}
	}
}

void testheap(void)
{
	//int i;
	//char *t;
	//kprintf("before char *t = kmalloc((size_t *)25):\n");
	//dump_heap();
	//t = kmalloc(25);
	//strcpy(t, "123456789012345678901234");
	//kprintf("after char *t = kmalloc((size_t *)25):\n");
	//dump_heap();
	//kfree(t);
	//kprintf("after kfree(t):\n");
	//dump_heap();
	//kprintf("before char *t = kmalloc((size_t *)25):\n");

	kprintf("Unable to run testheap -- kmalloc() is broken.\n");
}

void *krealloc(void *blk, size_t size)
{
	void *new_blk;
	malloc_t *m;

	/**
	 * size == 0: free block
	 */
	if(size == 0)
	{
		if(blk != NULL)
			kfree(blk);
		new_blk = NULL;
	}
	else
	{
		/**
		 * allocate new block
		 */
		new_blk = kmalloc(size);

		/**
		 * if allocation OK, and if old block exists, copy old block to new
		 */
		if(new_blk != NULL && blk != NULL)
		{
			m = (malloc_t *)((char *)blk - sizeof(malloc_t));
			if(m->magic != MALLOC_MAGIC)
			{
				/*printf("*** attempt to krealloc() block at 0x%p with bad magic value\n", blk);*/
				panic("attempt to realloc() block at 0x%p with bad magic value", blk);
				return NULL;
			}
			
			/**
			 * copy minimum of old and new block sizes
			 */
			if(size > m->size)
				size = m->size;
			memcpy(new_blk, blk, size);

			/**
			 * free the old block
			 */
			kfree(blk);
		}
	}
	return new_blk;
}

void keyboardISR(void);

int main(void)
{
	/**
	 * keyboard interrupt init
	 */
	vector_t v;
	unsigned i;

	init_video();
	init_keyboard();
	init_8259s();
	
	/**
	 * XXX:
	 * i know this is a very ugly way of doing this,
	 * however it is the only way it can be done for now.
	 * in the future, i will implement a kprintf function
	 * whose sole purpose will be writing boot messages.
	 *
	 * Also, the color codes need to be mapped to constants
	 * in order to make using them a hell of a lot easier.
	 */
	
	klog("init", "Installing keyboard interrupt handler", K_KLOG_PENDING, &_vc[0]);
		/* we don't save the old vector */
		v.eip = (unsigned)keyboard_irq;
		v.access_byte = 0x8E; /* present, ring 0, '386 interrupt gate */
		setvect(&v, 0x21);
	klog(NULL, NULL, K_KLOG_SUCCESS, &_vc[0]);

	/*init_tasks();*/

	klog("init", "Enabling hardware interrupts", K_KLOG_PENDING, &_vc[0]);
		enable();
		/*for(i = 0; i < 0xFFFFFFF; i++);*/
	klog(NULL, NULL, K_KLOG_SUCCESS, &_vc[0]);

	/**
	 * Initialize memory management
	 */
	/*_mm_init();*/

	/**
	 * finished init, time for some gooey ;)
	 */
	printf("                         _   _  _  _  ____  _____  ___                          ");
	printf("                        ( )_( )( \\/ )(  _ \\(  _  )/ __)                         ");
	printf("                         ) _ (  \\  /  ) _ < )(_)( \\__ \\                         ");
	printf("                        (_) (_) (__) (____/(_____)(___/                         \n");

	printf("                        Hybrid Operating System (HybOS)                         \n");

	/**
	 * XXX: debug only
	 */
	printf("ALT + F1 - F8 for virtual terminals\n");
	printf("Three finger salute to restart\n");
	printf("More work needs to be done\n");
	printf("$ ");

	/**
	 * fork (kfork()) control over to a shell
	 */
	/*init_shell();*/

	/**
	 * idle task/thread
	 */
	while(1)
	{
		schedule();
	}
	
	return 0;
}
