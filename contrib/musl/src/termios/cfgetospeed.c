#define _BSD_SOURCE
#include <termios.h>
#include <sys/ioctl.h>

speed_t cfgetospeed(const struct termios *tio)
{
	return tio->c_ospeed;
}

speed_t cfgetispeed(const struct termios *tio)
{
	return tio->c_ispeed;
}
