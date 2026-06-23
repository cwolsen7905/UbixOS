/* Tiny entry-point wrapper for bin/diff. */
#include "libbb.h"

extern int diff_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "diff";
	return diff_main(argc, argv);
}
