/* Tiny entry-point wrapper for bin/head. */
#include "libbb.h"

extern int head_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "head";
	return head_main(argc, argv);
}
