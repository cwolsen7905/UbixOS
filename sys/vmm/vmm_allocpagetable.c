#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <ubixos/spinlock.h>
#include <string.h>


int vmm_allocPageTable(uint32_t pdI, pidType pid) {
  uint32_t *pageDirectory = PD_BASE_ADDR;
  uint32_t *pageTable = 0x0;

  if ((pdI >= PD_ENTRIES) || ((pageDirectory[pdI] & PAGE_PRESENT) == PAGE_PRESENT))
    return(-1);


  /* Lock The Page Directory So We Dont Collide With Another Thread */
  //spinLock(&pdSpinLock);

  /* Map Page Table Page Into Page Directory */
  if ((pdI >= PD_INDEX(VMM_USER_START)) && (pdI <= PD_INDEX(VMM_USER_END)))
    pageDirectory[pdI] = (uint32_t) vmm_findFreePage(pid) | PAGE_DEFAULT;
  else
    pageDirectory[pdI] = (uint32_t) vmm_findFreePage(pid) | KERNEL_PAGE_DEFAULT;

  /* Map Page Table To Virtual Space So We Can Easily Manipulate It */
  pageTable = (uint32_t *) (PT_BASE_ADDR + (PD_INDEX( PT_BASE_ADDR ) * PAGE_SIZE));

  if ((pageTable[pdI] & PAGE_PRESENT) == PAGE_PRESENT)
    kpanic("How did this happen");

  pageTable[pdI] =  pageDirectory[pdI];
  
  /* Reload Page Directory */
  asm(
    "movl %cr3,%eax\n"
    "movl %eax,%cr3\n"
  );

  /* Clean The Page */
  pageTable = (uint32_t *) (PT_BASE_ADDR + (pdI * PAGE_SIZE));
  bzero(pageTable, PAGE_SIZE);

  //spinUnlock(&pdSpinLock);

  return(0x0);
  
}
