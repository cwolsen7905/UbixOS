/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86_64 machine-dependent loader part: copy the hand-off trampoline to low
 * memory and enter the kernel's 32-bit physical entry through it.
 */

#include "../stand.h"

extern const uint8_t ubix_tramp_start[], ubix_tramp_end[];

const uint16_t md_elf_machine = 62; /* EM_X86_64 */

static uint8_t *g_tramp;

/**
 * Copy the trampoline to a page below 4 GB (the 32-bit code must be
 * addressable and identity mapped).  @return 0 on success, -1 on failure.
 */
int md_prepare(efi_system_table_t *st)
{
	uint64_t a = 0xFFFFFFFFULL;

	if (st->boot_services->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_CODE, 1, &a) != EFI_SUCCESS)
		return -1;
	g_tramp = (uint8_t *)(uintptr_t)a;
	memcpy(g_tramp, ubix_tramp_start, (unsigned long long)(ubix_tramp_end - ubix_tramp_start));
	return 0;
}

/** Jump through the trampoline; see handoff.S. */
void md_handoff(uint64_t bootinfo, uint64_t entry)
{
	((void (*)(uint64_t, uint64_t))(uintptr_t)g_tramp)(bootinfo, entry);
	for (;;)
		;
}
