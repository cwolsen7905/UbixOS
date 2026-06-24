/* Tiny entry-point wrapper for bin/echo. */
#include "libbb.h"

extern int echo_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "echo";
	return echo_main(argc, argv);
}
