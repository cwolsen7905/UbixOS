#include <stdbool.h>

bool ispunct(const char c)
{
	return ((c >= '!' && c<= '/') || (c >= ':' && c<= '@') || (c >= '[' && c<= '`') || (c >= '{' && c<= '~'));
}
