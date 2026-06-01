/* Tiny entry-point wrapper for bin/sleep. */
#include "libbb.h"

extern int sleep_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "sleep";
	return sleep_main(argc, argv);
}
