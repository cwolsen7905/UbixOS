/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * clear(1): clear the terminal screen.
 *
 * The uBixOS terminal (and serial consoles) speak VT100/ANSI, so this writes
 * "cursor home + erase display + erase scrollback" directly; there is no
 * terminfo database to consult.
 */

#include <unistd.h>

/**
 * Clear the screen and scrollback of the controlling terminal.
 *
 * @return 0 on success, 1 if the write failed.
 */
int main(void)
{
	static const char seq[] = "\033[H\033[2J\033[3J";

	return (write(STDOUT_FILENO, seq, sizeof(seq) - 1) == (ssize_t)(sizeof(seq) - 1)) ? 0 : 1;
}
