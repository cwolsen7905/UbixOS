/* Tiny entry-point wrapper for bin/cmp. */
#include "libbb.h"

extern int cmp_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "cmp";
	return cmp_main(argc, argv);
}
