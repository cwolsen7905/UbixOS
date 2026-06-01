/* Tiny entry-point wrapper for bin/ls. */
#include "libbb.h"

extern int ls_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "ls";
	return ls_main(argc, argv);
}
