/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * faulttest — aarch64 bring-up test for kernel fault containment.  Deliberately
 * dereferences a bad pointer so the kernel takes a data abort while this process
 * is current.  A robust kernel must terminate *this process* and keep running,
 * not park the whole OS.  If the OS survives, the boot continues past this test;
 * if it parks, the boot stops here (the bug this guards against).
 */
#include <unistd.h>
#include <string.h>

static void out(const char *s)
{
	write(1, s, strlen(s));
}

int main(void)
{
	volatile int *bad = (volatile int *)0x1; /* not mapped — write faults */

	out("faulttest: writing through a bad pointer (the OS should kill just me)\n");
	*bad = 0x42;

	/* Reached only if the bad write did NOT fault — itself a bug. */
	out("faulttest: BUG — survived the bad write\n");
	return (0);
}
