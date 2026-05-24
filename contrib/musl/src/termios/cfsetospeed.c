#define _BSD_SOURCE
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>

int cfsetospeed(struct termios *tio, speed_t speed)
{
	tio->c_ospeed = speed;
	return 0;
}

int cfsetispeed(struct termios *tio, speed_t speed)
{
	tio->c_ispeed = speed;
	return 0;
}
