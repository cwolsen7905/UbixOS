/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * A CPU-bound EL0 program for the aarch64 bring-up: spin in a tight loop that
 * makes no syscalls, so the only way it can be interrupted is a timer IRQ taken
 * at EL0.  With preemptible EL0 wired (the 0x480 lower-EL IRQ vector + IRQ-
 * enabled SPSR) the loop completes and returns to the shell while the system
 * stays live; without it the first timer tick would fault into el1_invalid.
 * Built static against the aarch64 musl.
 */
#include <unistd.h>

int main(void)
{
	static const char start[] = "spin: CPU-bound loop, no syscalls (timer must preempt at EL0)...\n";
	write(1, start, sizeof(start) - 1);

	volatile unsigned long acc = 0;
	for (unsigned long i = 0; i < 200000000UL; i++)
		acc += i;

	static const char done[] = "spin: done — survived EL0 preemption.\n";
	write(1, done, sizeof(done) - 1);
	return (int)(acc & 1);
}
