#include <stdbool.h>

bool isalnum(const char c)
{
	return ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '1' && c <= '9'));
}
