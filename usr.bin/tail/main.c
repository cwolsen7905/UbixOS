/* Tiny entry-point wrapper for bin/tail. */
#include "libbb.h"

extern int tail_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "tail";
	return tail_main(argc, argv);
}
