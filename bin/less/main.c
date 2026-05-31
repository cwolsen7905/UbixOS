/* Tiny entry-point wrapper for bin/less. */
#include "libbb.h"

extern int less_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "less";
	return less_main(argc, argv);
}
