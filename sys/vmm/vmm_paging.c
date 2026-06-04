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

#include <sys/types.h>
#include <sys/errno.h>
#include <vmm/vmm.h>
#include <vmm/vm_filecache.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <ubixos/kpanic.h>
#include <ubixos/sched.h>
#include <ubixos/spinlock.h>
#include <string.h>
#include <assert.h>
#include <sys/descrip.h>
#include <ubixos/vitals.h>
#include <sys/klog.h>

u_int32_t *kernelPageDirectory = NULL; // Pointer To Kernel Page Directory

static struct spinLock g_fkp_spin_lock = SPIN_LOCK_INITIALIZER;

static struct spinLock g_rmp_spin_lock = SPIN_LOCK_INITIALIZER;

/*****************************************************************************************
 Function: int vmm_paging_init();

 Description: This Function Will Initialize The Operating Systems Paging
 Abilities.

 Notes:
 02/20/2004 - Looked Over Code And Have Approved Its Quality
 07/28/3004 - All pages are set for ring-0 only no more user accessable

 *****************************************************************************************/

int vmm_paging_init()
{
	u_int32_t i = 0;
	u_int32_t *page_table = NULL;

	/* Allocate A Page Of Memory For Kernels Page Directory */
	kernelPageDirectory = (u_int32_t *)vmm_find_free_page(sysID);

	if (kernelPageDirectory == NULL)
	{
		K_PANIC("Error: vmm_find_free_page Failed");
	} /* end if */

	/* Clear The Memory To Ensure There Is No Garbage */
	memset(kernelPageDirectory, 0, PAGE_SIZE);

	/* Allocate a page for the first 4MB of memory */
	page_table = (u_int32_t *)vmm_find_free_page(sysID);
	if (page_table == NULL)
	{
		K_PANIC("Error: vmm_find_free_page Failed");
	}

	/* Make Sure The Page Table Is Clean */
	memset(page_table, 0, PAGE_SIZE);

	kernelPageDirectory[0] = (u_int32_t)page_table | PAGE_DEFAULT;

	/*
	 * Identity-map the full first 4 MB (PD[0], all 1024 entries).
	 * The kernel loads at 0x300000 which is within this range.
	 * The page bitmap lives at 0x101000–0x201FFF; free pages begin at 0x202000.
	 */
	for (i = 0; i < PD_ENTRIES; i++)
	{
		page_table[i] = (u_int32_t)((i * 0x1000) | PAGE_DEFAULT);
	} /* end for */

	/* Allocate a page for the second 4MB of memory */
	page_table = (u_int32_t *)vmm_find_free_page(sysID);
	if (page_table == NULL)
	{
		K_PANIC("Error: vmm_find_free_page Failed");
	}

	/* Make Sure The Page Table Is Clean */
	memset(page_table, 0, PAGE_SIZE);

	kernelPageDirectory[1] = (u_int32_t)page_table | PAGE_DEFAULT;

	/*
	 * Create page tables for the top 1GB of VM space. This space is set aside
	 * for kernel space and will be shared with each process
	 */

	for (i = PD_INDEX(VMM_KERN_START); i <= PD_INDEX(VMM_KERN_END); i++)
	{
		page_table = (u_int32_t *)vmm_find_free_page(sysID);
		if (page_table == NULL)
		{
			K_PANIC("Error: vmm_find_free_page Failed");
		}

		/* Make Sure The Page Table Is Clean */
		memset(page_table, 0, PAGE_SIZE);

		/* Map In The Page Directory */
		kernelPageDirectory[i] = (u_int32_t)page_table | KERNEL_PAGE_DEFAULT | PAGE_GLOBAL;
	} /* end for */

	kernelPageDirectory[1023] = (u_int32_t)page_table | KERNEL_PAGE_DEFAULT;
	page_table = (u_int32_t *)(kernelPageDirectory[1023] & 0xFFFFF000);

	{
		u_int32_t kstk1 = vmm_find_free_page(sysID);
		u_int32_t kstk2 = vmm_find_free_page(sysID);
		if (!kstk1 || !kstk2)
		{
			K_PANIC("vmm_paging_init: no free pages for kernel stack");
		}
		page_table[1023] = (kstk1 | KERNEL_PAGE_DEFAULT | PAGE_STACK);
		page_table[1022] = (kstk2 | KERNEL_PAGE_DEFAULT | PAGE_STACK);
	}

	/*
	 * Map Page Tables Into VM Space
	 * The First Page Table (4MB) Maps To All Page Directories
	 */
	if (kernelPageDirectory[PD_INDEX(PT_BASE_ADDR)] == 0)
	{
		page_table = (u_int32_t *)vmm_find_free_page(sysID);
		if (page_table == NULL)
		{
			K_PANIC("Error: vmm_find_free_page Failed");
		}

		memset(page_table, 0, PAGE_SIZE);

		kernelPageDirectory[PD_INDEX(PT_BASE_ADDR)] = (u_int32_t)page_table | KERNEL_PAGE_DEFAULT;
	}

	page_table = (u_int32_t *)(kernelPageDirectory[PD_INDEX(PT_BASE_ADDR)] & 0xFFFFF000);

	for (i = 0; i < PD_ENTRIES; i++)
	{
		page_table[i] = kernelPageDirectory[i];
	} /* end for */

	/*
	 * Map Page Directory Into VM Space
	 * First Page After Page Tables
	 */
	if (kernelPageDirectory[PD_INDEX(PD_BASE_ADDR)] == 0)
	{
		page_table = (u_int32_t *)vmm_find_free_page(sysID);
		if (page_table == NULL)
		{
			K_PANIC("Error: vmm_find_free_page Failed");
		}

		memset(page_table, 0, PAGE_SIZE);

		kernelPageDirectory[PD_INDEX(PD_BASE_ADDR)] = (u_int32_t)page_table | KERNEL_PAGE_DEFAULT;
	}

	page_table = (u_int32_t *)(kernelPageDirectory[PD_INDEX(PD_BASE_ADDR)] & 0xFFFFF000);
	page_table[0] = (u_int32_t)kernelPageDirectory | KERNEL_PAGE_DEFAULT;

	/* Allocate New Stack Space */
	page_table = (u_int32_t *)vmm_find_free_page(sysID);
	if (page_table == NULL)
	{
		K_PANIC("ERROR: vmm_find_free_page Failed");
	}

	/* Now Lets Turn On Paging With This Initial Page Table */
	asm volatile("movl %0,%%eax          \n"
	             "movl %%eax,%%cr3       \n"
	             "movl %%cr0,%%eax       \n"
	             "orl  $0x80010000,%%eax \n" /* Turn on memory protection */
	             "movl %%eax,%%cr0       \n"
	             :
	             : "d"((u_int32_t *)(kernelPageDirectory)));

	/* Remap the page bitmap from its physical location (right after the kernel)
	 * into kernel virtual space at VMM_MMAP_ADDR_PMODE. */
	{
		u_int32_t bmap_end = vmm_bitmap_phys + numPages * sizeof(vmm_page_info_t);
		u_int32_t bmap_page;
		for (bmap_page = vmm_bitmap_phys; bmap_page < bmap_end; bmap_page += PAGE_SIZE)
		{
			u_int32_t virt = VMM_MMAP_ADDR_PMODE + (bmap_page - vmm_bitmap_phys);
			if (vmm_remap_page(bmap_page, virt, PAGE_DEFAULT, sysID, 0) == 0)
			{
				K_PANIC("vmmRemapPage failed\n");
			}
		}
	}

	/* Set New Address For Memory Map Since Its Relocation */
	vmmMemoryMap = (vmm_page_info_t *)VMM_MMAP_ADDR_PMODE;

	/* Print information on paging */
	kprintf("paging0: pd=0x%X isr=0x%X\n", kernelPageDirectory, &_vmm_page_fault);

	/* Return so we know everything went well */
	return 0;
} /* END */

/*****************************************************************************************
 Function: int vmmRemapPage(Physical Source,Virtual Destination)

 Description: This Function Will Remap A Physical Page Into Virtual Space

 Notes:
 07/29/02 - Rewrote This To Work With Our New Paging System
 07/30/02 - Changed Address Of Page Tables And Page Directory
 07/28/04 - If perms == 0x0 set to PAGE_DEFAULT

 *****************************************************************************************/
u_int32_t vmm_remap_page(u_int32_t source, u_int32_t dest, u_int32_t perms, pidType pid, int have_lock)
{

	u_int32_t dest_page_directory_index = 0, dest_page_table_index = 0;
	u_int32_t *page_dir = NULL, *page_table = NULL;

	if (pid < sysID)
	{
		kpanic("Invalid PID %i", pid);
	}

	if (source == 0)
	{
		K_PANIC("source == 0x0");
	}

	if (dest == 0)
	{
		K_PANIC("dest == 0x0");
	}

	if (have_lock == 0)
	{
		if (dest >= VMM_USER_START && dest <= VMM_USER_END)
		{
			spinLock(&g_rmp_spin_lock);
		}
		else
		{
			spinLock(&pdSpinLock);
		}
	}

	if (perms == 0)
	{
		perms = KERNEL_PAGE_DEFAULT;
	}

	/* Set Pointer page_directory To Point To The Virtual Mapping Of The Page Directory */
	page_dir = (u_int32_t *)PD_BASE_ADDR;

	/* Get Index Into The Page Directory */
	dest_page_directory_index = PD_INDEX(dest);

	if ((page_dir[dest_page_directory_index] & PAGE_PRESENT) != PAGE_PRESENT)
	{
		// kprintf("[Npd_i:0x%X]", dest_page_directory_index);
		vmm_alloc_page_table(dest_page_directory_index, pid);
	}

	/* Set Address To Page Table */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * dest_page_directory_index));

	/* Get The Index To The Page Table */
	dest_page_table_index = PT_INDEX(dest);

	/* If The Page Is Already Mapped, Handle COW Or Bail */
	if ((page_table[dest_page_table_index] & PAGE_PRESENT) == PAGE_PRESENT)
	{
		kprintf("[vmm_remap_page] page already mapped at 0x%X (pt_idx 0x%X)\n", dest, dest_page_table_index);
		klog(KLOG_WARNING,
		     "vmm_remap_page: page already mapped at 0x%X (pt_idx 0x%X)",
		     dest,
		     dest_page_table_index);

		if ((page_table[dest_page_table_index] & PAGE_STACK) == PAGE_STACK)
		{
			kprintf("[vmm_remap_page] stack page: [0x%X]\n", dest);
			klog(KLOG_DEBUG, "vmm_remap_page: stack page at 0x%X", dest);
		}

		if ((page_table[dest_page_table_index] & PAGE_COW) != PAGE_COW)
		{
			kprintf("[vmm_remap_page] not COW: [0x%X][0x%X]\n", dest, page_table[dest_page_table_index]);
			klog(KLOG_ERR,
			     "vmm_remap_page: non-COW page already present at 0x%X pte=0x%X",
			     dest,
			     page_table[dest_page_table_index]);
			source = 0;
			goto rmDone;
		}

		/* COW page: free the old physical frame before remapping */
		free_page(page_table[dest_page_table_index] & 0xFFFFF000);
	}

	/* Set The Source Address In The Destination */
	page_table[dest_page_table_index] = source | perms;

/* Reload The Page Table; */
rmDone:
	asm volatile("push %eax     \n"
	             "movl %cr3,%eax\n"
	             "movl %eax,%cr3\n"
	             "pop  %eax     \n");

	/* Return */
	if (have_lock == 0)
	{
		if (dest >= VMM_USER_START && dest <= VMM_USER_END)
		{
			spinUnlock(&g_rmp_spin_lock);
		}
		else
		{
			spinUnlock(&pdSpinLock);
		}
	}

	return source;
}

/* Map a physical MMIO page (e.g. framebuffer) to the same virtual address.
 * Unlike vmm_remap_page, this silently overwrites an existing mapping so
 * callers like ogDisplay_UbixOS::SetMode can be called more than once. */
int vmm_remap_io_page(u_int32_t phys, u_int32_t perms, pidType pid)
{
	u_int32_t pd_idx, pt_idx;
	u_int32_t *page_dir, *page_table;

	if (phys == 0)
	{
		K_PANIC("vmm_remap_io_page: phys == 0");
	}

	spinLock(&pdSpinLock);

	page_dir = (u_int32_t *)PD_BASE_ADDR;
	pd_idx = PD_INDEX(phys);

	if ((page_dir[pd_idx] & PAGE_PRESENT) != PAGE_PRESENT)
	{
		vmm_alloc_page_table(pd_idx, pid);
	}

	page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * pd_idx));
	pt_idx = PT_INDEX(phys);

	page_table[pt_idx] = phys | perms;

	asm volatile("push %eax     \n"
	             "movl %cr3,%eax\n"
	             "movl %eax,%cr3\n"
	             "pop  %eax     \n");

	spinUnlock(&pdSpinLock);
	return 0;
}

/************************************************************************

 Function: void *vmm_get_free_kernel_page(pidType pid, u_int16_t count);
 Description: Returns A Free Page Mapped To The VM Space
 Notes:

 07/30/02 - This Returns A Free Page In The Kernel Space

 ***********************************************************************/
void *vmm_get_free_kernel_page(pidType pid, u_int16_t count)
{
	u_int32_t pd_i = 0, pt_i = 0;
	int c = 0;

	u_int32_t *page_directory = (u_int32_t *)PD_BASE_ADDR;

	u_int32_t *page_table = NULL;

	u_int32_t start_address = 0;

	/* Lock The Page Dir & Tables For All Since Kernel Space Is Shared */
	spinLock(&pdSpinLock);

	/* Lets Search For A Free Page */
	for (pd_i = PD_INDEX(VMM_KERN_START); pd_i <= PD_INDEX(VMM_KERN_END); pd_i++)
	{

		if ((page_directory[pd_i] & PAGE_PRESENT) != PAGE_PRESENT)
		{
			if (vmm_alloc_page_table(pd_i, pid) == -1)
			{
				kpanic("Failed To Allocate Page Dir: 0x%X", pd_i);
			}
		}

		/* Set Page Table Address */
		page_table = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * pd_i));

		/* Loop Through The Page Table For A Free Page */
		for (pt_i = 0; pt_i < PT_ENTRIES; pt_i++)
		{

			/* Check For Unalocated Page */
			if ((page_table[pt_i] & PAGE_PRESENT) != PAGE_PRESENT)
			{
				if (start_address == 0)
				{
					start_address = ((pd_i * (PD_ENTRIES * PAGE_SIZE)) + (pt_i * PAGE_SIZE));
				}
				c++;
			}
			else
			{
				start_address = 0;
				c = 0;
			}

			if (c == count)
			{
				goto gotPages;
			}
		}
	}

	start_address = 0;
	goto noPagesAvail;

gotPages:
	for (c = 0; c < count; c++)
	{
		if ((vmm_remap_page(vmm_find_free_page(pid),
		                   (start_address + (PAGE_SIZE * c)),
		                   KERNEL_PAGE_DEFAULT,
		                   pid,
		                   1)) == 0)
		{
			K_PANIC("vmmRemapPage failed: gfkp-1\n");
		}

		vmm_clear_virtual_page(start_address + (PAGE_SIZE * c));
	}

noPagesAvail:
	spinUnlock(&pdSpinLock);

	return (void *)start_address;
}

/************************************************************************

 Function: void vmm_clear_virtual_page(u_int32_t page_addr);

 Description: This Will Null Out A Page Of Memory

 Notes:

 ************************************************************************/
int vmm_clear_virtual_page(u_int32_t page_addr)
{
	u_int32_t *src = NULL;
	u_int32_t counter = 0;

	/* Set Source Pointer To Virtual Page Address */
	src = (u_int32_t *)page_addr;

	/* Clear Out The Page */
	for (counter = 0; counter < PD_ENTRIES; counter++)
	{
		src[counter] = 0;
	}

	/* Return */
	return 0;
}

void *vmm_map_from_task(pidType pid, void *ptr, u_int32_t size)
{
	kTask_t *child = NULL;
	u_int32_t i = 0, x = 0, y = 0, count = ((size + 4095) / 0x1000), c = 0;
	u_int32_t d_i = 0, t_i = 0;
	u_int32_t base_addr = 0, offset = 0;
	u_int32_t *child_page_dir = (u_int32_t *)VMM_CHILD_PD_WINDOW;
	u_int32_t *child_page_table = NULL;
	u_int32_t *page_table_src = NULL;
	offset = (u_int32_t)ptr & 0xFFF;
	base_addr = (u_int32_t)ptr & 0xFFFFF000;
	child = schedFindTask(pid);
	if (child == NULL)
	{
		kprintf("vmm_map_from_task: pid %i not found\n", pid);
		return NULL;
	}

	// Calculate The Page Table Index And Page Directory Index
	d_i = (base_addr / (1024 * 4096));
	t_i = ((base_addr - (d_i * (1024 * 4096))) / 4096);

	kprintf("cr3: 0x%X\n", child->md.md_tss.cr3);
	if (vmm_remap_page(child->md.md_tss.cr3, VMM_CHILD_PD_WINDOW, KERNEL_PAGE_DEFAULT, _current->id, 0) == 0)
	{
		K_PANIC("vmm_remap_page: Failed");
	}

	for (i = 0; i < PD_ENTRIES; i++)
	{

		if ((child_page_dir[i] & PAGE_PRESENT) == PAGE_PRESENT)
		{
			// kprintf("mapping: 0x%X\n", i);
			if (vmm_remap_page(
			        child_page_dir[i], 0x5A01000 + (i * 0x1000), KERNEL_PAGE_DEFAULT, _current->id, 0) ==
			    0)
			{
				K_PANIC("Returned NULL");
			}
		}
	}
	kprintf("mapping completed\n");
	// for (x = (_current->oInfo.vmStart / (1024 * 4096)); x < 1024; x++) {
	for (x = PD_INDEX(VMM_KERN_START); x < PD_INDEX(VMM_KERN_END); x++)
	{
		// kpanic("v_mFT");
		page_table_src = (u_int32_t *)(PT_BASE_ADDR + (4096 * x));

		for (y = 0; y < 1024; y++)
		{

			// Loop Through The Page Table Find An UnAllocated Page
			if (page_table_src[y] == 0)
			{

				if (count > 1)
				{

					for (c = 0; ((c < count) && (y + c < 1024)); c++)
					{

						if (page_table_src[y + c] != 0)
						{

							c = -1;
							break;
						}
					}

					if (c != -1)
					{

						for (c = 0; c < count; c++)
						{

							if ((t_i + c) >= 0x1000)
							{

								d_i++;
								t_i = 0 - c;
							}

							if ((child_page_dir[d_i] & PAGE_PRESENT) == PAGE_PRESENT)
							{
								// kprintf("d_i: 0x%X\n", d_i);
								child_page_table =
								    (u_int32_t *)(0x5A01000 + (0x1000 * d_i));

								if ((child_page_table[t_i + c] & PAGE_PRESENT) ==
								    PAGE_PRESENT)
								{
									if (vmm_remap_page(child_page_table[t_i + c],
									                  ((x * (1024 * 4096)) +
									                   ((y + c) * 4096)),
									                  KERNEL_PAGE_DEFAULT,
									                  _current->id,
									                  0) == 0)
									{
										K_PANIC("remap == NULL");
									}
								}
							}
						}

						vmm_unmap_page(VMM_CHILD_PD_WINDOW, VMM_KEEP);

						for (i = 0; i < 0x1000; i++)
						{
							vmm_unmap_page((0x5A01000 + (i * 0x1000)), VMM_KEEP);
						}

						return ((void *)((x * (1024 * 4096)) + (y * 4096) + offset));
					}
				}
				else
				{

					// Map A Physical Page To The Virtual Page
					child_page_table = (u_int32_t *)(0x5A01000 + (0x1000 * d_i));
					if ((child_page_dir[d_i] & PAGE_PRESENT) == PAGE_PRESENT)
					{
						// kprintf("eDI: 0x%X", d_i);
						if ((child_page_table[t_i] & PAGE_PRESENT) == PAGE_PRESENT)
						{
							if (vmm_remap_page(child_page_table[t_i],
							                  ((x * (1024 * 4096)) + (y * 4096)),
							                  KERNEL_PAGE_DEFAULT,
							                  _current->id,
							                  0) == 0)
							{
								K_PANIC("remap Failed");
							}
						}
					}

					// Return The Address Of The Mapped In Memory
					vmm_unmap_page(VMM_CHILD_PD_WINDOW, VMM_KEEP);

					for (i = 0; i < 0x1000; i++)
					{
						vmm_unmap_page((0x5A01000 + (i * 0x1000)), VMM_KEEP);
					}

					return ((void *)((x * (1024 * 4096)) + (y * 4096) + offset));
				}
			}
		}
	}

	return NULL;
}

void *vmm_get_free_malloc_page(u_int16_t count)
{
	u_int32_t x = 0, y = 0;
	int c = 0;
	u_int32_t *page_table_src = NULL;
	u_int32_t *page_directory = NULL;

	page_directory = (u_int32_t *)PD_BASE_ADDR;

	spinLock(&g_fkp_spin_lock);

	/* Lets Search For A Free Page */
	for (x = PD_INDEX(VMM_KERN_START); x <= PD_INDEX(VMM_KERN_END); x++)
	{
		if ((page_directory[x] & PAGE_PRESENT) !=
		    PAGE_PRESENT) /* If Page Directory Is Not Yet Allocated Allocate It */
		{
			vmm_alloc_page_table(x, sysID);
		}

		page_table_src = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * x));

		for (y = 0; y < 1024; y++)
		{
			/* Loop Through The Page Table Find An UnAllocated Page */
			if (page_table_src[y] == 0)
			{
				if (count > 1)
				{
					for (c = 0; c < count; c++)
					{
						if (y + c < 1024)
						{
							if (page_table_src[y + c] != 0)
							{
								c = -1;
								break;
							}
						}
						else
						{
							c = -1;
							break;
						}
					}
					if (c != -1)
					{
						for (c = 0; c < count; c++)
						{
							if (vmm_remap_page(vmm_find_free_page(sysID),
							                  ((x * 0x400000) + ((y + c) * 0x1000)),
							                  KERNEL_PAGE_DEFAULT,
							                  sysID,
							                  0) == 0)
							{
								K_PANIC("remap Failed");
							}

							vmm_clear_virtual_page((x * 0x400000) + ((y + c) * 0x1000));
						}
						spinUnlock(&g_fkp_spin_lock);
						return ((void *)((x * (PAGE_SIZE * PD_ENTRIES)) + (y * PAGE_SIZE)));
					}
				}
				else
				{
					/* Map A Physical Page To The Virtual Page */
					if (vmm_remap_page(vmm_find_free_page(sysID),
					                  ((x * 0x400000) + (y * 0x1000)),
					                  KERNEL_PAGE_DEFAULT,
					                  sysID,
					                  0) == 0)
					{
						K_PANIC("Failed");
					}

					/* Clear This Page So No Garbage Is There */
					vmm_clear_virtual_page((x * 0x400000) + (y * 0x1000));
					/* Return The Address Of The Newly Allocate Page */
					spinUnlock(&g_fkp_spin_lock);
					return ((void *)((x * (PAGE_SIZE * PD_ENTRIES)) + (y * PAGE_SIZE)));
				}
			}
		}
	}
	/* If No Free Page Was Found Return NULL */
	spinUnlock(&g_fkp_spin_lock);
	return NULL;
}

int obreak(struct thread *td, struct obreak_args *uap)
{
	u_int32_t i = 0;
	vm_offset_t old = 0;
	vm_offset_t base = 0;
	vm_offset_t new = 0;

	base = round_page((vm_offset_t)td->vm_daddr);
	old = base + ctob(td->vm_dsize);

	if (old < VMM_USER_START)
	{
		kprintf("obreak: vm_daddr=0x%X vm_dsize=%u — not initialized\n",
		        (u_int32_t)td->vm_daddr,
		        (u_int32_t)td->vm_dsize);
		td->td_retval[0] = -1;
		return (EINVAL);
	}

	/* brk(0): return current break (Linux ABI compatible) */
	if (uap->nsize == 0)
	{
		td->td_retval[0] = (int)old;
		return 0;
	}

	new = round_page((vm_offset_t)uap->nsize);

	/* Invalid address below data segment — return old break to signal failure */
	if (new < base)
	{
		td->td_retval[0] = (int)old;
		return 0;
	}

	if (new > old)
	{
		for (i = old; i < new; i += 0x1000)
		{
			if (vmm_remap_page(vmm_find_free_page(_current->id), i, PAGE_DEFAULT, _current->id, 0) == 0)
			{
				td->td_retval[0] = (int)old;
				return 0;
			}
		}
		td->vm_dsize += btoc(new - old);
		td->td_retval[0] = (int)new;
	}
	else if (new < old)
	{
		u_int32_t *page_dir = (u_int32_t *)PD_BASE_ADDR;
		u_int32_t *page_table = NULL;

		for (i = new; i < old; i += PAGE_SIZE)
		{
			u_int32_t pd_idx = PD_INDEX(i);
			u_int32_t pt_idx = PT_INDEX(i);

			if ((page_dir[pd_idx] & PAGE_PRESENT) != PAGE_PRESENT)
			{
				continue;
			}

			page_table = (u_int32_t *)(PT_BASE_ADDR + (pd_idx * PAGE_SIZE));

			if ((page_table[pt_idx] & PAGE_PRESENT) != PAGE_PRESENT)
			{
				continue;
			}

			u_int32_t phys = page_table[pt_idx] & 0xFFFFF000;

			if ((page_table[pt_idx] & PAGE_COW) == PAGE_COW)
			{
				adjust_cow_counter(phys, -1);
			}
			else if ((phys >> 12) < numPages)
			{
				free_page(phys);
			}

			page_table[pt_idx] = 0;
		}

		td->vm_dsize -= btoc(old - new);

		asm volatile("movl %%cr3, %%eax\n\t"
		             "movl %%eax, %%cr3\n\t"
		             :
		             :
		             : "eax", "memory");

		td->td_retval[0] = (int)new;
	}
	else
	{
		td->td_retval[0] = (int)old;
	}

	return 0;
}

int vmm_clean_virtual_space(u_int32_t addr)
{
	u_int32_t x = 0;
	u_int32_t y = 0;

	u_int32_t *page_table_src = NULL;
	u_int32_t *page_dir = NULL;

	page_dir = (u_int32_t *)PD_BASE_ADDR;

	for (x = PD_INDEX(addr); x <= PD_INDEX(VMM_USER_END); x++)
	{
		if ((page_dir[x] & PAGE_PRESENT) == PAGE_PRESENT)
		{
			page_table_src = (u_int32_t *)(PT_BASE_ADDR + (PAGE_SIZE * x));

			for (y = 0; y < PT_ENTRIES; y++)
			{
				if ((page_table_src[y] & PAGE_PRESENT) == PAGE_PRESENT)
				{
					u_int32_t phys = page_table_src[y] & 0xFFFFF000;
					if ((page_table_src[y] & PAGE_COW) == PAGE_COW)
					{
						adjust_cow_counter(phys, -1);
					}
					else if ((page_table_src[y] & PAGE_SHARED) == PAGE_SHARED)
					{
						/* Shared page: file-page cache or vmm_share_region.  Drop
						 * a file-cache reference (frees on the last one); a
						 * share_region page isn't in the cache → no-op, its owner
						 * frees it. */
						vm_filecache_unref_phys(phys);
					}
					else if ((phys >> 12) < numPages)
					{
						free_page(phys);
					}
					page_table_src[y] = 0;
				}
			}
		}
	}

	asm("movl %cr3,%eax\n"
	    "movl %eax,%cr3\n");

#ifdef VMM_DEBUG
	kprintf("Here!?");
#endif

	return 0;
}
