/* Tiny entry-point wrapper for bin/uname. */
#include "libbb.h"

extern int uname_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "uname";
	return uname_main(argc, argv);
}
