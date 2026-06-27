/* Tiny entry-point wrapper for bin/install. */
#include "libbb.h"

extern int install_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "install";
	return install_main(argc, argv);
}
