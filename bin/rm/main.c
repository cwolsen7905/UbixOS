/* Tiny entry-point wrapper for bin/rm. */
#include "libbb.h"

extern int rm_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "rm";
	return rm_main(argc, argv);
}
