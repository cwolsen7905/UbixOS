/* Tiny entry-point wrapper for bin/cat. */
#include "libbb.h"

extern int cat_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "cat";
	return cat_main(argc, argv);
}
