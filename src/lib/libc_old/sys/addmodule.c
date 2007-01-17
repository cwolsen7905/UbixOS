#include <stdlib.h>


int AddModule(const char *file, const char *name)
{
	asm volatile(
	"int %0\n"
	: : "i" (0x80), "a" (37), "b" (file), "c" (name)
	);
	return 1;
}
