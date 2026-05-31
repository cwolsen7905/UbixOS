/* Tiny entry-point wrapper for bin/tr. */
#include "libbb.h"

extern int tr_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "tr";
	return tr_main(argc, argv);
}
