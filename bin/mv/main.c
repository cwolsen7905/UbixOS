/* Tiny entry-point wrapper for bin/mv. */
#include "libbb.h"

extern int mv_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "mv";
	return mv_main(argc, argv);
}
