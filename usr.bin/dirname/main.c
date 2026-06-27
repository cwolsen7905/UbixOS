/* Tiny entry-point wrapper for bin/dirname. */
#include "libbb.h"

extern int dirname_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "dirname";
	return dirname_main(argc, argv);
}
