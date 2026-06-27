/* Tiny entry-point wrapper for bin/find. */
#include "libbb.h"

extern int find_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "find";
	return find_main(argc, argv);
}
