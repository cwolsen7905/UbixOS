/* Tiny entry-point wrapper for bin/wc. */
#include "libbb.h"

extern int wc_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "wc";
	return wc_main(argc, argv);
}
