/* Tiny entry-point wrapper for bin/expr. */
#include "libbb.h"

extern int expr_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "expr";
	return expr_main(argc, argv);
}
