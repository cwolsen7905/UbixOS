/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * reboot(8) — restart the machine.  Invokes reboot(2) (FreeBSD syscall 55, which
 * the aarch64 kernel services via PSCI SYSTEM_RESET) so the firmware/bootloader
 * runs again: U-Boot re-loads the kernel from the FAT /boot partition and boots
 * it.  This is the "reboot" step of the on-device self-rebuild loop — build a new
 * kernel, install it to /boot, then `reboot` into it.
 *
 * The number is passed literally (not musl's SYS_reboot, whose bits/syscall.h
 * mapping is inconsistent) so it matches the kernel's FreeBSD-ABI dispatch.
 */
#include <unistd.h>
#include <stdio.h>

#define SYS_REBOOT_FREEBSD 55
#define RB_AUTOBOOT 0

int main(void)
{
	printf("Rebooting...\n");
	fflush(stdout);
	syscall(SYS_REBOOT_FREEBSD, RB_AUTOBOOT);
	/* Only reached if the syscall returned (it should not on success). */
	fprintf(stderr, "reboot: failed\n");
	return (1);
}
