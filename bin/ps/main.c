/* Tiny entry-point wrapper for bin/ps. */
#include "libbb.h"

extern int ps_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "ps";
	return ps_main(argc, argv);
}
