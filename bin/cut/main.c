/* Tiny entry-point wrapper for bin/cut. */
#include "libbb.h"

extern int cut_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "cut";
	return cut_main(argc, argv);
}
