/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Interface between the machine-independent loader (loader.c) and the
 * per-architecture part (stand/efi/<arch>/).
 */

#ifndef _STAND_STAND_H
#define _STAND_STAND_H

#include "efi.h"

/** ELF e_machine value the kernel image must carry (EM_X86_64 = 62). */
extern const uint16_t md_elf_machine;

/**
 * Prepare the final hand-off (allocate and fill any low-memory trampoline).
 * Called while boot services are still available.
 * @return 0 on success, -1 on failure.
 */
int md_prepare(efi_boot_services_t *bs);

/**
 * Enter the kernel at physical address @entry with the boot-info block at
 * physical @bootinfo.  Boot services are gone; never returns.
 */
void md_handoff(uint64_t bootinfo, uint64_t entry) __attribute__((noreturn));

void *memcpy(void *d, const void *s, unsigned long long n);
void *memset(void *d, int c, unsigned long long n);

#endif /* _STAND_STAND_H */
