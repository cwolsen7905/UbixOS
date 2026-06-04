/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * RNG smoke test: exercises the kernel CSPRNG through the standard FreeBSD/
 * POSIX interfaces — getrandom(2) (syscall 563) and /dev/urandom — proving both
 * front-ends work and produce non-trivial, non-repeating output in-OS.
 */

#include <sys/random.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * @return 0 if both RNG paths return varied, non-constant bytes; 1 otherwise.
 */
int
main(void)
{
	unsigned char a[32], b[32], c[32];
	int fail = 0, fd, i, allzero;

	/* 1) getrandom(2): two draws must differ and not be all-zero. */
	if (getrandom(a, sizeof(a), 0) != (ssize_t)sizeof(a) ||
	    getrandom(b, sizeof(b), 0) != (ssize_t)sizeof(b)) {
		printf("getrandom: syscall FAIL\n");
		fail = 1;
	} else {
		allzero = 1;
		for (i = 0; i < 32; i++)
			if (a[i] != 0)
				allzero = 0;
		if (allzero || memcmp(a, b, 32) == 0) {
			printf("getrandom: weak (allzero=%d identical=%d) FAIL\n", allzero,
			    memcmp(a, b, 32) == 0);
			fail = 1;
		} else {
			printf("getrandom: PASS\n");
		}
	}

	/* 2) /dev/urandom: must read and differ from the getrandom draw. */
	fd = open("/dev/urandom", O_RDONLY);
	if (fd < 0 || read(fd, c, sizeof(c)) != (ssize_t)sizeof(c)) {
		printf("/dev/urandom: open/read FAIL\n");
		fail = 1;
	} else if (memcmp(c, a, 32) == 0) {
		printf("/dev/urandom: identical to getrandom FAIL\n");
		fail = 1;
	} else {
		printf("/dev/urandom: PASS\n");
	}
	if (fd >= 0)
		close(fd);

	printf(fail ? "rndtest: FAIL\n" : "rndtest: PASS\n");
	return (fail);
}
