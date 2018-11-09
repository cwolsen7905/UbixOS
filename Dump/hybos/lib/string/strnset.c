#include <string.h> /* size_t */

char *_strnset(char * string, int val, size_t count)
{
	char *start = string;

	while(count-- && *string)
		*string++ = (char)val;

	return start;
}
