/**
 * printf.c
 *
 */

#include <stdarg.h> /* va_* */
#include <_printf.h> /* fnptr_t */
#include <stdio.h>

int do_printf(const char *fmt, va_list args, fnptr_t fn, void *ptr);
void putch(unsigned c);

int printf_help(unsigned c, void **ptr)
{
	/**
	 * Leave this for now
	 */
	ptr = ptr;

	putch(c);
	return 0;
}

void printf(const char *fmt, ...)
{
	va_list args;

	va_start(args, fmt);
	(void)do_printf(fmt, args, printf_help, NULL);
	va_end(args);
}

