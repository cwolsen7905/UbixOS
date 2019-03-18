#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <_printf.h> /* do_printf() */
#include "_krnl.h"
#include "bootlog.h"

void klog(char *proc, char *entry, KLOGRESULT result, console_t *vtty0)
{
	unsigned oldattrib;
	int i = 0;
	int offset = 69; /* -4 for the "[  ]" part, -2 for the ": " part, -1 for space at end, and -4 for the status */
	char status[4];
	va_list args;

	args = args;
	status[0] = '\0';

	/**
	 * Save our old color attributes
	 */
	oldattrib = vtty0->attrib;
	
	if(result == K_KLOG_SUCCESS)
	{
		/**
		 * Successfull initialization of something.
		 * Write "..Ok" then leave
		 */
		vtty0->attrib = 8;
		printf("\b\b\b\b..");
		vtty0->attrib = 2;
		printf("Ok\n");
		vtty0->attrib = oldattrib;

		return;
	}
	else if(result == K_KLOG_FAILURE)
	{
		/**
		 * Unsuccessfull initialization of something.
		 * Write "Fail" then leave
		 */
		vtty0->attrib = 4;
		printf("\b\b\b\bFail\n");
		vtty0->attrib = oldattrib;

		return;
	}

	/**
	 * FIXME
	 *
	 * Should "wrap" the line instead
	 */
	if(strlen(entry) + 8 > 80)
		return;

	vtty0->attrib = 8;
	printf("[ ");
	vtty0->attrib = 15;
	printf("%s: %s", proc, entry);
	vtty0->attrib = 8;	
	printf(" ]");

	offset -= strlen(proc);
	offset -= strlen(entry);

	for(i = 0; i < offset; i++)
		printf(".");

	vtty0->attrib = 8;
	printf("Wait");

	vtty0->attrib = oldattrib;
}

