/* Tiny entry-point wrapper for bin/touch. */
#include "libbb.h"

extern int touch_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "touch";
	return touch_main(argc, argv);
}
