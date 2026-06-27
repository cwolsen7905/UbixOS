/* Tiny entry-point wrapper for bin/stat. */
#include "libbb.h"

extern int stat_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "stat";
	return stat_main(argc, argv);
}
