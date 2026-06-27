/* Tiny entry-point wrapper for bin/top. */
#include "libbb.h"

extern int top_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "top";
	return top_main(argc, argv);
}
