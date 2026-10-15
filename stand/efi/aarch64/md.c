/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * AArch64 machine-dependent loader part: locate the device tree, copy the
 * hand-off trampoline to low memory and enter the kernel through it.
 */

#include "../stand.h"

extern const uint8_t ubix_tramp_start[], ubix_tramp_end[];

const uint16_t md_elf_machine = 183; /* EM_AARCH64 */

static uint8_t *g_tramp;
static uint64_t g_dtb;

/** EFI_DTB_TABLE_GUID b1b621d5-f19c-41a5-830b-d9152c69aae0 */
static const efi_guid_t g_guid_dtb = {0xB1B621D5, 0xF19C, 0x41A5, {0x83, 0x0B, 0xD9, 0x15, 0x2C, 0x69, 0xAA, 0xE0}};

/**
 * Find the firmware device tree and copy the trampoline to a page below 4 GB.
 * @return 0 on success, -1 if no low page, -2 if no DTB (the kernel sizes RAM
 * from it).
 */
int md_prepare(efi_system_table_t *st)
{
	uint64_t a = 0xFFFFFFFFULL;
	uintn_t i;

	for (i = 0; i < st->number_of_table_entries; i++)
	{
		const efi_guid_t *g = &st->configuration_table[i].guid;
		const uint8_t *x = (const uint8_t *)g, *y = (const uint8_t *)&g_guid_dtb;
		int j, same = 1;

		for (j = 0; j < (int)sizeof(*g); j++)
			if (x[j] != y[j])
				same = 0;
		if (same)
			g_dtb = (uint64_t)(uintptr_t)st->configuration_table[i].table;
	}
	if (g_dtb == 0)
		return -2;
	if (st->boot_services->allocate_pages(EFI_ALLOCATE_MAX_ADDRESS, EFI_LOADER_CODE, 1, &a) != EFI_SUCCESS)
		return -1;
	g_tramp = (uint8_t *)(uintptr_t)a;
	memcpy(g_tramp, ubix_tramp_start, (unsigned long long)(ubix_tramp_end - ubix_tramp_start));
	return 0;
}

/** Jump through the trampoline; see handoff.S. */
void md_handoff(uint64_t bootinfo, uint64_t entry)
{
	((void (*)(uint64_t, uint64_t, uint64_t))(uintptr_t)g_tramp)(bootinfo, entry, g_dtb);
	for (;;)
		;
}
