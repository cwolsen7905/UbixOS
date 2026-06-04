/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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
#include <vmm/paging.h>
#include <vmm/vm_filecache.h>

/************************************************************************

 Function: void vmm_unmap_page(u_int32_t page_addr,int flags);
 Description: This Function Will Unmap A Page From The Kernel VM Space
 The Flags Variable Decides If Its To Free The Page Or Not
 A Flag Of 0 Will Free It And A Flag Of 1 Will Keep It
 Notes:

 07/30/02 - I Have Decided That This Should Free The Physical Page There
 Is No Reason To Keep It Marked As Not Available

 07/30/02 - Ok A Found A Reason To Keep It Marked As Available I Admit
 Even I Am Not Perfect Ok The Case Where You Wouldn't Want To
 Free It Would Be On Something Like Where I Allocated A Page
 To Create A New Virtual Space So Now It Has A Flag

 ************************************************************************/
void vmm_unmap_page(u_int32_t page_addr, unmapFlags_t flags)
{
	u_int32_t page_directory_index = 0, page_table_index = 0;
	u_int32_t *page_table = NULL;
	u_int32_t *page_directory = NULL;

	page_directory = (u_int32_t *)PD_BASE_ADDR;

	/* Get The Index To The Page Directory */
	page_directory_index = (page_addr >> 22);

	if ((page_directory[page_directory_index] & PAGE_PRESENT) !=
	    PAGE_PRESENT) // NOLINT(clang-analyzer-core.FixedAddressDereference)
	{
		return;
	}

	// Calculate The Page Table Index
	page_table_index = ((page_addr >> 12) & 0x3FF);

	/* Set page_table To The Virtual Address Of Table */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (0x1000 * page_directory_index));

	/* Free The Physical Page If Flags Is 0, guarding non-present and MMIO frames */
	if (flags == 0 && (page_table[page_table_index] & PAGE_PRESENT))
	{
		u_int32_t phys = page_table[page_table_index] & 0xFFFFF000;
		if ((page_table[page_table_index] & PAGE_SHARED))
		{
			/* Shared page (file-page cache or vmm_share_region) — never freed
			 * directly here.  unref_phys frees it iff this was the last
			 * file-cache reference; a share_region page isn't in the cache and
			 * is a no-op (its owner frees it). */
			vm_filecache_unref_phys(phys);
		}
		else if ((phys >> 12) < numPages)
		{
			free_page(phys);
		}
	}

	/* Unmap The Page */
	page_table[page_table_index] = 0;

	/* Rehash The Page Directory */
	asm volatile("movl %cr3,%eax\n"
	             "movl %eax,%cr3\n");

	/* Return */
	return;
}

/************************************************************************

 Function: void vmm_unmap_pages(u_int32_t page_addr,int flags);
 Description: This Function Will Unmap A Page From The Kernel VM Space
 The Flags Variable Decides If Its To Free The Page Or Not
 A Flag Of 0 Will Free It And A Flag Of 1 Will Keep It
 Notes:

 07/30/02 - I Have Decided That This Should Free The Physical Page There
 Is No Reason To Keep It Marked As Not Available

 07/30/02 - Ok A Found A Reason To Keep It Marked As Available I Admit
 Even I Am Not Perfect Ok The Case Where You Wouldn't Want To
 Free It Would Be On Something Like Where I Allocated A Page
 To Create A New Virtual Space So Now It Has A Flag

 ************************************************************************/
void vmm_unmap_pages(void *ptr, u_int32_t size, unmapFlags_t flags)
{
	u_int32_t addr = (u_int32_t)ptr & 0xFFFFF000;
	u_int32_t end = addr + (((size + 4095) / 4096) * 4096);

	while (addr < end)
	{
		vmm_unmap_page(addr, flags);
		addr += 4096;
	}
	return;
}
