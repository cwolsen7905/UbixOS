/* Tiny entry-point wrapper for bin/grep. */
#include "libbb.h"

extern int grep_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "grep";
	return grep_main(argc, argv);
}
