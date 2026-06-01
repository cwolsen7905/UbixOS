/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES ARE DISCLAIMED.
 */

#ifndef VMM_SWAP_H
#define VMM_SWAP_H

#include <sys/types.h>

/* 64 MB swap area: 16384 pages × 4 KB = 64 MB */
#define SWAP_PAGES              16384U
#define SWAP_SECTORS_PER_PAGE   8U      /* 4096 / 512 */

struct driveInfo;

/* Called by the disk driver when a swap partition (type 0x82) is found. */
void swap_register_dev(struct driveInfo *hdd, u_int32_t total_pages);

/* Returns non-zero if a swap device has been registered. */
int swap_enabled(void);

u_int32_t swap_alloc_slot(void);
void     swap_free_slot(u_int32_t slot);
int      swap_write_page(u_int32_t slot, void *virt_addr);
int      swap_read_page(u_int32_t slot, void *virt_addr);

/*
 * Try to evict one user page from _current's address space.
 * On success, writes the page to swap, updates the PTE, calls free_page,
 * and returns the physical address of the reclaimed page.
 * Returns 0 if no page could be evicted.
 */
u_int32_t swap_evict_page(void);

#endif /* VMM_SWAP_H */
