#include <stdbool.h>

bool isascii(const unsigned char c)
{
	return (c >= 0x00 && c <= 0x7F);
}
