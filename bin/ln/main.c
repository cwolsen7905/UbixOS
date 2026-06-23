/* Tiny entry-point wrapper for bin/ln. */
#include "libbb.h"

extern int ln_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "ln";
	return ln_main(argc, argv);
}
