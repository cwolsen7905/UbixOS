/*
 * clear — clear the terminal screen.
 *
 * Emits the VT100/ANSI "cursor home" + "erase display" sequence, which the
 * UbixOS console and the GUI terminal both interpret.
 *
 * (C) 2002-2026 The UbixOS Project
 */
#include <unistd.h>

/**
 * Write the cursor-home + erase-display escape sequence to stdout.
 *
 * @return 0 on success, 1 if the write failed.
 */
int main(void)
{
	static const char seq[] = "\033[H\033[2J";

	return (write(STDOUT_FILENO, seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1)) ? 0 : 1;
}
