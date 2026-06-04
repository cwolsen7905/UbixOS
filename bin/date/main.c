/* Tiny entry-point wrapper for bin/date. */
#include "libbb.h"

extern int date_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "date";
	return date_main(argc, argv);
}
