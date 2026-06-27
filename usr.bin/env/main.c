/* Tiny entry-point wrapper for bin/env. */
#include "libbb.h"

extern int env_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "env";
	return env_main(argc, argv);
}
