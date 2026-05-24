#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <ubixos/spinlock.h>
#include <ubixos/kpanic.h>
#include <string.h>

int vmm_allocPageTable(u_int32_t pd_i, pidType pid)
{
	u_int32_t *page_directory = (u_int32_t *)PD_BASE_ADDR;
	u_int32_t *page_table = 0x0;

	if ((pd_i >= PD_ENTRIES) || ((page_directory[pd_i] & PAGE_PRESENT) == PAGE_PRESENT))
	{
		return (-1);
	}

	/* Lock The Page Directory So We Dont Collide With Another Thread */
	// spinLock(&pdSpinLock);

	/* Map Page Table Page Into Page Directory */
	if ((pd_i >= PD_INDEX(VMM_USER_START)) && (pd_i <= PD_INDEX(VMM_USER_END)))
	{
		page_directory[pd_i] = (u_int32_t)vmm_findFreePage(pid) | PAGE_DEFAULT;
	}
	else
	{
		page_directory[pd_i] = (u_int32_t)vmm_findFreePage(pid) | KERNEL_PAGE_DEFAULT;
	}

	/* Map Page Table To Virtual Space So We Can Easily Manipulate It */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (PD_INDEX(PT_BASE_ADDR) * PAGE_SIZE));

	if ((page_table[pd_i] & PAGE_PRESENT) == PAGE_PRESENT)
	{
		kpanic("How did this happen");
	}

	page_table[pd_i] = page_directory[pd_i];

	/* Reload Page Directory */
	asm("movl %cr3,%eax\n"
	    "movl %eax,%cr3\n");

	/* Clean The Page */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (pd_i * PAGE_SIZE));
	bzero(page_table, PAGE_SIZE);

	// spinUnlock(&pdSpinLock);

	return (0x0);
}
