/* Tiny entry-point wrapper for bin/sort. */
#include "libbb.h"

extern int sort_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "sort";
	return sort_main(argc, argv);
}
