/* Tiny entry-point wrapper for bin/more. */
#include "libbb.h"

extern int more_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "more";
	return more_main(argc, argv);
}
