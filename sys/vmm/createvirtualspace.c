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
#include <string.h>

/************************************************************************

 Function: void *vmm_createVirtualSpace(pid_t);
 Description: Creates A Virtual Space For A New Task
 Notes:

 07/30/02 - This Is Going To Create A New VM Space However Its Going To
 Share The Same Top 1GB Space With The Kernels VM And Lower
 1MB Of VM Space With The Kernel

 07/30/02 - Note This Is Going To Get The Top 1Gig And Lower 1MB Region
 From The Currently Loaded Page Directory This Is Safe Because
 All VM Spaces Will Share These Regions

 07/30/02 - Note I Realized A Mistake The First Page Table Will Need To Be
 A Copy But The Page Tables For The Top 1GB Will Not Reason For
 This Is That We Just Share The First 1MB In The First Page Table
 So We Will Just Share Physical Pages.

 08/02/02 - Added Passing Of pid_t pid For Better Tracking Of Who Has Which
 Set Of Pages

 ************************************************************************/
void *vmm_createVirtualSpace(pid_t pid)
{

	void *new_page_directory_address = 0x0;
	u_int32_t *parent_page_directory = 0x0, *new_page_directory = 0x0;
	u_int32_t *parent_page_table = 0x0, *new_page_table = 0x0;

	int x = 0;

	/* Set Address Of Parent Page Directory */
	parent_page_directory = (u_int32_t *)PD_BASE_ADDR; /* PD_BASE_ADDR is the addressable Page Directory */

	/* Allocate A New Page For The New Page Directory */
	new_page_directory = (u_int32_t *)vmm_getFreePage(pid);

	/* Set new_page_directory_address To The Newly Created Page Directories Page */
	new_page_directory_address = (void *)vmm_getPhysicalAddr((u_int32_t)new_page_directory);

	/* First Set Up A Flushed Page Directory */
	bzero(new_page_directory, PAGE_SIZE);

	/* Map The Lower 8MB Kernel Code Space */
	/* Map First 4MB 1:1 */
	new_page_directory[0] = parent_page_directory[0];
	// XXX: We Dont Need This - new_page_directory[1] = parent_page_directory[1];

	/* Map Second 4MB */
	new_page_table = (u_int32_t *)vmm_getFreePage(pid);
	bzero(new_page_table, PAGE_SIZE);

	parent_page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * 1));

	for (x = 0; x < PT_ENTRIES; x++)
	{
		if (((parent_page_table[x]) & PAGE_PRESENT) == PAGE_PRESENT)
		{

			/* Set Page To COW In Parent And Child Space */
			new_page_table[x] =
			    (((u_int32_t)parent_page_table[x] & 0xFFFFF000) | ((PAGE_DEFAULT & ~PAGE_WRITE) | PAGE_COW));

			/* Increment The COW Counter For This Page */
			if (((u_int32_t)parent_page_table[x] & PAGE_COW) == PAGE_COW)
			{
				adjustCowCounter(((u_int32_t)parent_page_table[x] & 0xFFFFF000), 1);
			}
			else
			{
				/* Add Two If This Is The First Time Setting To COW */
				adjustCowCounter(((u_int32_t)parent_page_table[x] & 0xFFFFF000), 2);
				parent_page_table[x] |= PAGE_COW; // new_page_table[i];
			}
		}
		else {
			new_page_table[x] = parent_page_table[x];
		}
	}

	new_page_directory[1] = (vmm_getPhysicalAddr((u_int32_t)new_page_table) | KERNEL_PAGE_DEFAULT);

	vmm_unmapPage((u_int32_t)new_page_table, 1);
	/* Done Mapping Second 4MB */

	/* Map The Top Kernel (APPROX 1GB) Region Of The VM Space */
	for (x = PD_INDEX(VMM_KERN_START); x < PD_ENTRIES; x++)
	{
		new_page_directory[x] = parent_page_directory[x];
	}

	/* Allocate Stack Pages */
	new_page_table = (u_int32_t *)vmm_getFreePage(pid);
	bzero(new_page_table, PAGE_SIZE);

	new_page_directory[1023] = (vmm_getPhysicalAddr((u_int32_t)new_page_table) | KERNEL_PAGE_DEFAULT);

	parent_page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * 1023));
	new_page_table[1023] = parent_page_table[1023] | PAGE_COW;
	adjustCowCounter(((u_int32_t)parent_page_table[1023] & 0xFFFFF000), 2);
	new_page_table[1022] = parent_page_table[1022] | PAGE_COW;
	adjustCowCounter(((u_int32_t)parent_page_table[1022] & 0xFFFFF000), 2);

	vmm_unmapPage((u_int32_t)new_page_table, 1);

	/*
	 * Allocate A New Page For The The First Page Table Where We Will Map The
	 * Lower Region
	 */

	// new_page_table = (u_int32_t *) vmm_getFreePage(pid);
	/* Flush The Page From Garbage In Memory */
	// bzero(new_page_table, PAGE_SIZE);
	/* Map This Into The Page Directory */
	// new_page_directory[0] = (vmm_getPhysicalAddr((u_int32_t) new_page_table) | KERNEL_PAGE_DEFAULT); //MrOlsen
	// 2018-01-14 PAGE_DEFAULT
	/* Set Address Of Parents Page Table */
	// parent_page_table = (u_int32_t *) PT_BASE_ADDR;
	/* Map The First 1MB Worth Of Pages */
	/*
	 for (x = 0; x < (PD_ENTRIES / 4); x++) {
	 new_page_table[x] = parent_page_table[x];
	 }
	 */

	/* Unmap Page From Virtual Space */
	// vmm_unmapPage((u_int32_t) new_page_table, 1);
	/*
	 *
	 * Map Page Directory Into VM Space
	 * First Page After Page Tables
	 * This must be mapped into the page directory before we map all 1024 page directories into the memory space
	 */
	new_page_table = (u_int32_t *)vmm_getFreePage(pid);

	new_page_directory[PD_INDEX(PD_BASE_ADDR)] = (u_int32_t)(vmm_getPhysicalAddr((u_int32_t)new_page_table) |
	                                                        KERNEL_PAGE_DEFAULT); // MrOlsen 2018-01-14 PAGE_DEFAULT

	new_page_table[0] =
	    (u_int32_t)((u_int32_t)(new_page_directory_address) | KERNEL_PAGE_DEFAULT); // MrOlsen 2018-01-14 PAGE_DEFAULT

	vmm_unmapPage((u_int32_t)new_page_table, 1);

	/*
	 *
	 * Map Page Tables Into VM Space
	 * The First Page Table (4MB) Maps To All Page Directories
	 *
	 */

	new_page_table = (u_int32_t *)vmm_getFreePage(pid);

	new_page_directory[PD_INDEX(PT_BASE_ADDR)] = (u_int32_t)(vmm_getPhysicalAddr((u_int32_t)new_page_table) |
	                                                        KERNEL_PAGE_DEFAULT); // MrOlsen 2018-01-14 PAGE_DEFAULT

	/* Flush The Page From Garbage In Memory */
	bzero(new_page_table, PAGE_SIZE);

	for (x = 0; x < PD_ENTRIES; x++)
	{
		new_page_table[x] = new_page_directory[x];
	}

	/* Unmap Page From Virtual Space */
	vmm_unmapPage((u_int32_t)new_page_table, 1);

	/* Now We Are Done With The Page Directory So Lets Unmap That Too */
	vmm_unmapPage((u_int32_t)new_page_directory, 1);

	/* Return Physical Address Of Page Directory */
	return (new_page_directory_address);
}
