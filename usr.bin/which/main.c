/* Tiny entry-point wrapper for bin/which. */
#include "libbb.h"

extern int which_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "which";
	return which_main(argc, argv);
}
