/* Tiny entry-point wrapper for bin/mkdir. */
#include "libbb.h"

extern int mkdir_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "mkdir";
	return mkdir_main(argc, argv);
}
