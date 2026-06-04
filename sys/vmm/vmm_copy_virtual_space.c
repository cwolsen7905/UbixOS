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
#include <vmm/vm_filecache.h>
#include <vmm/paging.h>
#include <sys/kern_sysctl.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <string.h>

static struct spinLock g_cvs_spin_lock = SPIN_LOCK_INITIALIZER;

/************************************************************************

 Function: void *vmm_copy_virtual_space(pidType pid);

 Description: Creates A Copy Of A Virtual Space And Set All NON Kernel
 Space To COW For A Fork This Will Also Alter The Parents
 VM Space To Make That COW As Well

 Notes:

 08/02/02 - Added Passing Of pidType pid So We Can Better Keep Track Of
 Which Task Has Which Physical Pages

 ************************************************************************/
void *vmm_copy_virtual_space(pidType pid)
{
	void *new_page_directory_address = NULL;

	u_int32_t *parent_page_directory = NULL, *new_page_directory = NULL;
	u_int32_t *parent_page_table = NULL, *new_page_table = NULL;
	u_int32_t *parent_stack_page = NULL, *new_stack_page = NULL;
	u_int32_t x = 0, i = 0;

	spinLock(&g_cvs_spin_lock);

	/* Set Address Of Parent Page Directory */
	parent_page_directory = (u_int32_t *)PD_BASE_ADDR;

	/* Allocate A New Page For The New Page Directory */
	new_page_directory = vmm_get_free_kernel_page(pid, 1);
	if (new_page_directory == NULL)
	{
		kpanic("Error: new_page_directory == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
	}

	/* Set new_page_directory_address To The Newly Created Page Directories Page */
	new_page_directory_address = (void *)vmm_get_physical_addr((u_int32_t)new_page_directory);

	/* First Set Up A Flushed Page Directory */
	memset(new_page_directory, 0, PAGE_SIZE);

	/* Map Kernel Code Region Entries 0 & 1 */
	new_page_directory[0] = parent_page_directory[0]; // NOLINT(clang-analyzer-core.FixedAddressDereference)
	// XXX: We Dont Need This - new_page_directory[1] = parent_page_directory[1];

	new_page_table = vmm_get_free_kernel_page(pid, 1);
	if (new_page_table == NULL)
	{
		kpanic("Error: new_page_table == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
	}

	parent_page_table = (u_int32_t *)(PT_BASE_ADDR + PAGE_SIZE);

	for (x = 0; x < PT_ENTRIES; x++)
	{
		if (((parent_page_table[x]) & PAGE_PRESENT) == PAGE_PRESENT)
		{

			{
				u_int32_t phys = parent_page_table[x] & 0xFFFFF000;

				/* MMIO pages — share as-is, no COW */
				if ((phys >> 12) >= numPages)
				{
					new_page_table[x] = parent_page_table[x];
				}
				else if ((parent_page_table[x] & PAGE_SHARED) == PAGE_SHARED)
				{
					/* Shared file-cache (or share_region) page: share as-is into
					 * the child — never COW it (it is read-only and not tracked
					 * by the COW counter).  Bump the file-cache refcount for the
					 * child's mapping; a share_region page isn't in the cache, so
					 * the ref is a no-op. */
					new_page_table[x] = parent_page_table[x];
					vm_filecache_ref_phys(phys);
				}
				else
				{
					/* Set Page To COW In Parent And Child Space — clear PAGE_WRITE so
					 * the CPU faults on write and the COW handler fires. */
					new_page_table[x] = (phys | ((PAGE_DEFAULT & ~PAGE_WRITE) | PAGE_COW));

					/* Increment The COW Counter For This Page */
					if ((parent_page_table[x] & PAGE_COW) == PAGE_COW)
					{
						adjust_cow_counter(phys, 1);
					}
					else
					{
						/* Add Two If This Is The First Time Setting To COW */
						adjust_cow_counter(phys, 2);
						parent_page_table[x] = (parent_page_table[x] & ~PAGE_WRITE) | PAGE_COW;
					}
				}
			}
		}
		else
		{
			new_page_table[x] = parent_page_table[x];
		}
	}

	new_page_directory[1] = (vmm_get_physical_addr((u_int32_t)new_page_table) | KERNEL_PAGE_DEFAULT);

	vmm_unmap_page((u_int32_t)new_page_table, VMM_KEEP);

	new_page_table = NULL;

	/* Map The Kernel Memory Region Entry 770 Address 0xC0800000 */
	for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++)
	{
		new_page_directory[x] = parent_page_directory[x];
	}

	/* Map The Kernel Stack Region */
	for (x = PD_INDEX(VMM_KERN_STACK_START); x <= PD_INDEX(VMM_KERN_STACK_END); x++)
	{
		if ((parent_page_directory[x] & PAGE_PRESENT) == PAGE_PRESENT)
		{
			/* Set Parent To Propper Page Table */
			parent_page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * x));

			/* Allocate A New Page Table */
			new_page_table = vmm_get_free_kernel_page(pid, 1);
			if (new_page_table == NULL)
			{
				kpanic("Error: new_page_table == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
			}

			memset(new_page_table, 0, PAGE_SIZE);

			for (i = 0; i < PT_ENTRIES; i++)
			{
				if ((parent_page_table[i] & PAGE_PRESENT) == PAGE_PRESENT)
				{

					/* Alloc A New Page For This Stack Page */
					new_stack_page = vmm_get_free_kernel_page(pid, 1);
					if (new_stack_page == NULL)
					{
						kpanic("Error: new_stack_page == NULL, File: %s, Line: %i\n",
						       __FILE__,
						       __LINE__);
					}

					/* Set Pointer To Parents Stack Page */
					parent_stack_page = (u_int32_t *)((u_int32_t)PAGE_SIZE * PD_ENTRIES * x +
					                                  (u_int32_t)PAGE_SIZE * i);

					/* Copy The Stack Byte For Byte (I Should Find A Faster Way) */
					memcpy(new_stack_page, parent_stack_page, PAGE_SIZE);

					/* Insert New Stack Into Page Table */
					new_page_table[i] = (vmm_get_physical_addr((u_int32_t)new_stack_page) |
					                     PAGE_DEFAULT | PAGE_STACK);

					/* Unmap From Kernel Space */
					vmm_unmap_page((u_int32_t)new_stack_page, VMM_KEEP);
				}
			}
			/* Put New Page Table Into New Page Directory */
			new_page_directory[x] = (vmm_get_physical_addr((u_int32_t)new_page_table) | PAGE_DEFAULT);
			/* Unmap Page From Kernel Space But Keep It Marked As Not Avail */
			vmm_unmap_page((u_int32_t)new_page_table, VMM_KEEP);
		}
	}

	/*
	 * Now For The Fun Stuff For Page Tables 2-767 We Must Map These And Set
	 * The Permissions On Every Mapped Pages To COW This Will Conserve Memory
	 * Because The Two VM Spaces Will Be Sharing Pages Unless an EXECVE Happens
	 *
	 * We start at the 4MB boundary as the first 4MB is special
	 */

	for (x = PD_INDEX(VMM_USER_START); x <= PD_INDEX(VMM_USER_END); x++)
	{

		/* If Page Table Exists Map It */
		if ((parent_page_directory[x] & PAGE_PRESENT) == PAGE_PRESENT)
		{

			/* Set Parent To Propper Page Table */
			parent_page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * x));

			/* Allocate A New Page Table */
			new_page_table = vmm_get_free_kernel_page(pid, 1);
			if (new_page_table == NULL)
			{
				kpanic("Error: new_page_table == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
			}

			memset(new_page_table, 0, PAGE_SIZE);

			/* Set Parent And New Pages To COW */
			for (i = 0; i < PT_ENTRIES; i++)
			{

				/* If Page Is Mapped */
				if ((parent_page_table[i] & PAGE_PRESENT) == PAGE_PRESENT)
				{

					/* Check To See If Its A Stack Page */
					if ((parent_page_table[i] & PAGE_STACK) == PAGE_STACK)
					{

						/* Alloc A New Page For This Stack Page */
						new_stack_page = vmm_get_free_kernel_page(pid, 1);
						if (new_stack_page == NULL)
						{
							kpanic("Error: new_stack_page == NULL, File: %s, Line: %i\n",
							       __FILE__,
							       __LINE__);
						}

						/* Set Pointer To Parents Stack Page */
						parent_stack_page =
						    (u_int32_t *)((u_int32_t)PAGE_SIZE * PD_ENTRIES * x +
						                  (u_int32_t)PAGE_SIZE * i);

						/* Copy The Stack Byte For Byte (I Should Find A Faster Way) */
						memcpy(new_stack_page, parent_stack_page, PAGE_SIZE);

						/* Insert New Stack Into Page Table */
						new_page_table[i] = (vmm_get_physical_addr((u_int32_t)new_stack_page) |
						                     PAGE_DEFAULT | PAGE_STACK);

						/* Unmap From Kernel Space */
						vmm_unmap_page((u_int32_t)new_stack_page, VMM_KEEP);
					}
					else
					{
						u_int32_t phys = parent_page_table[i] & 0xFFFFF000;

						/* MMIO/device pages (framebuffer etc.) live above RAM.
						 * They must not be COW'd — share the mapping as-is so
						 * the parent keeps write access after the fork. */
						if ((phys >> 12) >= numPages)
						{
							new_page_table[i] = parent_page_table[i];
						}
						else
						{
							/* Set Page To COW In Parent And Child Space — clear PAGE_WRITE
							 * so the CPU faults on write and the COW handler fires. */
							new_page_table[i] =
							    (phys | ((PAGE_DEFAULT & ~PAGE_WRITE) | PAGE_COW));

							/* Increment The COW Counter For This Page */
							if ((parent_page_table[i] & PAGE_COW) == PAGE_COW)
							{
								adjust_cow_counter(phys, 1);
							}
							else
							{
								/* Add Two If This Is The First Time Setting To COW */
								adjust_cow_counter(phys, 2);
								parent_page_table[i] =
								    (parent_page_table[i] & ~PAGE_WRITE) | PAGE_COW;
							}
						}
					}
				}
				else
				{
					new_page_table[i] = 0;
				}
			}

			/* Put New Page Table Into New Page Directory */
			new_page_directory[x] = (vmm_get_physical_addr((u_int32_t)new_page_table) | PAGE_DEFAULT);
			/* Unmap Page From Kernel Space But Keep It Marked As Not Avail */
			vmm_unmap_page((u_int32_t)new_page_table, VMM_KEEP);
		}
	}

	/*
	 * Allocate A New Page For The The First Page Table Where We Will Map The
	 * Lower Region First 4MB
	 */

	/*
	 *
	 * Map Page Directory Into VM Space
	 * First Page After Page Tables
	 * This must be mapped into the page directory before we map all 1024 page directories into the memory space
	 */
	new_page_table = vmm_get_free_kernel_page(pid, 1);

	new_page_directory[PD_INDEX(PD_BASE_ADDR)] =
	    vmm_get_physical_addr((u_int32_t)new_page_table) | PAGE_DEFAULT;

	new_page_table[0] = (u_int32_t)new_page_directory_address | PAGE_DEFAULT;

	vmm_unmap_page((u_int32_t)new_page_table, VMM_KEEP);

	/*
	 *
	 * Map Page Tables Into VM Space
	 * The First Page Table (4MB) Maps To All Page Directories
	 *
	 */

	new_page_table = vmm_get_free_kernel_page(pid, 1);

	new_page_directory[PD_INDEX(PT_BASE_ADDR)] =
	    vmm_get_physical_addr((u_int32_t)new_page_table) | PAGE_DEFAULT;

	/* Flush The Page From Garbage In Memory */
	memset(new_page_table, 0, PAGE_SIZE);

	/*
	 * Re-sync kernel PD entries into new_page_directory now that all
	 * vmm_get_free_kernel_page / vmm_get_free_page calls are done.  Those
	 * calls may have added new PD entries to the kernel range that were
	 * not present when the first copy was done above.  PD_BASE_ADDR and
	 * PT_BASE_ADDR (indices 768-769) are below VMM_KERN_START (770) so
	 * they are NOT overwritten by this loop.
	 */
	for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++)
	{
		new_page_directory[x] = parent_page_directory[x];
	}

	for (x = 0; x < PD_ENTRIES; x++)
	{
		new_page_table[x] = new_page_directory[x];
	}

	/* Unmap Page From Virtual Space */
	vmm_unmap_page((u_int32_t)new_page_table, VMM_KEEP);

	/* Now We Are Done With The Page Directory So Lets Unmap That Too */
	vmm_unmap_page((u_int32_t)new_page_directory, VMM_KEEP);

	spinUnlock(&g_cvs_spin_lock);

	/* Return Physical Address Of Page Directory */
	return (new_page_directory_address);
}
