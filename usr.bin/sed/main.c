/* Tiny entry-point wrapper for bin/sed. */
#include "libbb.h"

extern int sed_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "sed";
	return sed_main(argc, argv);
}
