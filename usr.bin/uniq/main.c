/* Tiny entry-point wrapper for bin/uniq. */
#include "libbb.h"

extern int uniq_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "uniq";
	return uniq_main(argc, argv);
}
