/* Tiny entry-point wrapper for bin/vi.  Sets applet_name (used by libbb's
 * error helpers) and dispatches to busybox vi_main(). */
#include "libbb.h"

extern int vi_main(int argc, char **argv);

int main(int argc, char **argv)
{
	applet_name = "vi";
	return vi_main(argc, argv);
}
