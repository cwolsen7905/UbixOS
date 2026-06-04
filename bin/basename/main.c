/* Tiny entry-point wrapper for bin/basename. */
#include "libbb.h"

extern int basename_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "basename";
	return basename_main(argc, argv);
}
