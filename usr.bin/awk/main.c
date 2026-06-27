/* Tiny entry-point wrapper for bin/awk. */
#include "libbb.h"

extern int awk_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "awk";
	return awk_main(argc, argv);
}
