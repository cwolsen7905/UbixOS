/*============================================================================
DEBUG FUNCTIONS

EXPORTS:
void dump_regs(regs_t *regs);
============================================================================*/
#include "_krnl.h"

/* IMPORTS
from MAIN.C */
void printf(const char *fmt, ...);
/*****************************************************************************
*****************************************************************************/
#define BPERL		16	/* byte/line for dump */

void dump(unsigned char *data, unsigned count)
{
	unsigned char byte1, byte2;

	while(count != 0)
	{
		for(byte1 = 0; byte1 < BPERL; byte1++)
		{
			if(count == 0)
				break;
			printf("%02X ", data[byte1]);
			count--;
		}
		printf("\t");
		for(byte2 = 0; byte2 < byte1; byte2++)
		{
			if(data[byte2] < ' ')
				printf("%c", '.');
			else
				printf("%c", data[byte2]);
		}
		printf("\n");
		data += BPERL;
	}
}
/*****************************************************************************
*****************************************************************************/
void dump_regs(regs_t *regs)
{
	printf("EDI=%08X ESI=%08X EBP=%08X ESP=%08X\n",
		regs->edi, regs->esi, regs->ebp, regs->esp);
	printf("EBX=%08X EDX=%08X ECX=%08X EAX=%08X\n",
		regs->ebx, regs->edx, regs->ecx, regs->eax);
	printf(" DS=%08X  ES=%08X  FS=%08X  GS=%08X\n",
		regs->ds, regs->es, regs->fs, regs->gs);
	printf("int=%08X err=%08X EIP=%08X  CS=%08X\n",
		regs->which_int, regs->err_code, regs->eip, regs->cs);
	printf("uSP=%08X uSS=%08X\n", regs->user_esp, regs->user_ss);
}
