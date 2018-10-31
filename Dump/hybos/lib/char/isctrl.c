#include <stdbool.h>

bool isctrl(const char c)
{
	return (!(c >= ' ' && c <= '~'));
}
