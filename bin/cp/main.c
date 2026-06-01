/* Tiny entry-point wrapper for bin/cp. */
#include "libbb.h"

extern int cp_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "cp";
	return cp_main(argc, argv);
}
