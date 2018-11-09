/**
 * sprintf.c
 *
 */

#include <stdarg.h> /* va_* */

int vsprintf_help(unsigned c, void **ptr);

int sprintf(char *buffer, const char *fmt, ...)
{
	va_list args;
	int ret_val;

	va_start(args, fmt);
	ret_val = vsprintf(buffer, fmt, args);
	va_end(args);
	return ret_val;
}
