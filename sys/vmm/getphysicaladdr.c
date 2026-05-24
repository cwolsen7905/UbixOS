/*-
 * Copyright (c) 2002-2018 The UbixOS Project.
 * All rights reserved.
 *
 * This was developed by Christopher W. Olsen for the UbixOS Project.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted
 * provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice, this list of
 *    conditions, the following disclaimer and the list of authors in the documentation and/or
 *    other materials provided with the distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <vmm/vmm.h>

/*!
 Function: void *vmm_getPhysicalAddr();
 Description: Returns The Physical Address Of The Virtual Page
 Notes:
 */

/* returns the real address of page is page aligned */
u_int32_t vmm_getPhysicalAddr(u_int32_t page_addr)
{
	int page_directory_index = 0x0, page_table_index = 0x0;
	u_int32_t *page_table = 0x0;

	// Calculate The Page Directory Index
	page_directory_index = (page_addr >> 22);

	// Calculate The Page Table Index
	page_table_index = ((page_addr >> 12) & 0x3FF);

	/* Set page_table To The Virtual Address Of Table */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (0x1000 * page_directory_index));

	/* Return The Physical Address Of The Page */
	return ((u_int32_t)(page_table[page_table_index] & 0xFFFFF000));
}

/* Returns the real address not page aligned */
u_int32_t vmm_getRealAddr(u_int32_t addr)
{
	int page_directory_index = 0x0, page_table_index = 0x0;
	u_int32_t *page_table = 0x0;

	// Calculate The Page Directory Index
	page_directory_index = (addr >> 22);

	// Calculate The Page Table Index
	page_table_index = ((addr >> 12) & 0x3FF);

	/* Set page_table To The Virtual Address Of Table */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (0x1000 * page_directory_index));
	/* Return The Physical Address Of The Page */
	return ((u_int32_t)(page_table[page_table_index] & 0xFFFFF000) + (addr & 0xFFF));
}
