#ifndef __TL__KRNL_H
#define	__TL__KRNL_H

//#ifdef __cplusplus
//extern "C"
//{
//#endif

#include <setjmp.h> /* jmp_buf */

#define	MAX_VC	12 /* maximum number of virtual terminals */

/**
 * CPU error codes
 */
#define	_K_CPU_DBZ			0		/* Divide by zero */
#define	_K_CPU_SS			1		/* Single Step */
#define	_K_CPU_NMI			2		/* Non-maskable (NMI) */
#define	_K_CPU_BPX			3		/* Breakpoint */
#define	_K_CPU_OFT			4		/* Overflow trap */
#define	_K_CPU_BND			5		/* BOUND range exceeded */
#define	_K_CPU_IOP			6		/* Invalid opcode */
#define	_K_CPU_CNA			7		/* Coprocessor not available */
#define	_K_CPU_DFE			8		/* Double fault exception */
#define	_K_CPU_CSO			9		/* Coprocessor segment overrun */
#define	_K_CPU_CPE			10		/* Coprocessor error */
#define	_K_CPU_ITS			0x0A	/* Invalid task state management */
#define	_K_CPU_SNP			0x0B	/* Segment not present */
#define	_K_CPU_SEX			0x0C	/* Stack exception (or illicit sex) */
#define	_K_CPU_GPF			0x0D	/* General protection exception */
#define	_K_CPU_PGF			0x0E	/* Page fault */

/**
 * Interrupt table
 */
#define	_K_IRQ_TIMER		0x08	/* timer (55ms intervals, 18.21590/sec) */
#define	_K_IRQ_KEYBOARD	0x09	/* keyboard service required */
#define	_K_IRQ_S8259		0x0A	/* slave 8259 or EGA/VGA vertical retrace */
#define	_K_IRQ_COM2			0x0B	/* COM2 service required (PS/2 MCA COM3-COM8) */
#define	_K_IRQ_COM1			0x0C	/* COM1 service required */
#define	_K_IRQ_HDDREQ		0x0D	/* fixed disk or data request from LPT2 */
#define	_K_IRQ_FDDSERV		0x0E	/* floppy disk service required */
#define	_K_IRQ_LPT1REQ		0x0F	/* data request from LPT1 */
#define	_K_IRQ_VIDEO		0x10	/* video (int 10h) */
#define	_K_IRQ_EQPDET		0x11	/* equipment determination (int 11h) */
#define	_K_IRQ_MEMORY		0x12	/* memory size (int 12h) */
#define	_K_IRQ_DIO			0x13	/* disk I/O service (int 13h) */
#define	_K_IRQ_SERIAL		0x14	/* serial communications (int 14h) */
#define	_K_IRQ_SYSTEM		0x15	/* system services, cassette (int 15h) */
#define	_K_IRQ_KBDSERV		0x16	/* keyboard services (int 16h) */
#define	_K_IRQ_LPT			0x17	/* parallel printer (int 17h) */
#define	_K_IRQ_ROM			0x18	/* ROM BASIC loader */
#define	_K_IRQ_BTSTRAP		0x19	/* bootstrap loader (unreliable, int 19h) */
#define	_K_IRQ_TOD			0x1A	/* time of day (int 1A) */
#define	_K_IRQ_CBREAK		0x1B	/* user defined ctrl-break handler (int 1B) */
#define	_K_IRQ_TICK			0x1C	/* user defined clock tick handler (int 1C) */
#define	_K_IRQ_VIDEOP		0x1D	/* 6845 video parameter pointer */
#define	_K_IRQ_DISKPARAM	0x1E	/* diskette parameter pointer (base table) */
#define	_K_IRQ_GCHTBL		0x1F	/* graphics character table */
#define	_K_IRQ_HDD			0x40	/* hard disk */
#define	_K_IRQ_FIXEDD0		0x41	/* fixed disk 0 parameters pointer (int 13h, int 9h) */
#define	_K_IRQ_RELVID		0x42	/* relocated video handler (EGA/VGA/PS) */
#define	_K_IRQ_UFT			0x43	/* user font table (EGA/VGA/PS) */
#define	_K_IRQ_GRAPH		0x44	/* first 128 graphics characters (also Netware) */
#define	_K_IRQ_FIXED1		0x46	/* fixed disk 1 parameters ptr (int 13h, int 9h/int 41h) */
#define	_K_IRQ_PCJR			0x48	/* PCjr cordless keyboard translation */
#define	_K_IRQ_PCJRSCAN	0x49	/* PCjr non-keyboard scancode translation table */
#define	_K_IRQ_USERALARM	0x4A	/* user alarm (int 4A) */
#define	_K_IRQ_PALARM		0x50	/* periodic alarm timer (PS/2) */
#define	_K_IRQ_GSS			0x59	/* GSS computer graphics interface */
#define	_K_IRQ_BIOSEP		0x5A	/* cluster adapter BIOS entry point */
#define	_K_IRQ_CADAPBT		0x5B	/* cluster adapter boot */
#define	_K_IRQ_NETBIOS		0x5C	/* NETBIOS interface, TOPS interface */
#define	_K_IRQ_EMS			0x67	/* LIM/EMS specifications (int 67h) */
#define	_K_IRQ_APPC			0x68	/* APPC */
#define	_K_IRQ_RTC			0x70	/* real time clock (int 15h) */
#define	_K_IRQ_REDIRIRQ2	0x71	/* software redirected to IRQ2 */
#define	_K_IRQ_MOUSE		0x74	/* mouse interrupt */
#define	_K_IRQ_HDC			0x76	/* fixed disk controller */

/* the code for setvect() and getvect() in
KSTART.ASM depends on the layout of this structure */
typedef struct
{
	unsigned access_byte, eip;
} vector_t;

/* the layout of this structure must match the order of registers
pushed and popped by the exception handlers in KSTART.ASM */
typedef struct
{
/* pushed by pusha */
	unsigned edi, esi, ebp, esp, ebx, edx, ecx, eax;
/* pushed separately */
	unsigned ds, es, fs, gs;
	unsigned which_int, err_code;
/* pushed by exception. Exception may also push err_code.
user_esp and user_ss are pushed only if a privilege change occurs. */
	unsigned eip, cs, eflags, user_esp, user_ss;
} regs_t;

typedef struct	/* circular queue */
{
	unsigned char *data;
	unsigned size, in_base, in_ptr/*, out_base*/, out_ptr;
} queue_t;

typedef struct
{
/* virtual console input */
	queue_t keystrokes;
/* virtual console output */
	unsigned esc, attrib, csr_x, csr_y, esc1, esc2, esc3;
	unsigned short *fb_adr;
} console_t;

typedef struct
{
	console_t *vc;
	jmp_buf state;
	enum
	{
		TS_RUNNABLE = 1, TS_BLOCKED = 2, TS_ZOMBIE = 3
	} status;
} task_t;

#ifdef __cplusplus
}
#endif

#endif
