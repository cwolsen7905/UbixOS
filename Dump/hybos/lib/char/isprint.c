#include <stdbool.h>

bool isprint(const char c)
{
	return (c >= ' ' && c <= '~');
}
