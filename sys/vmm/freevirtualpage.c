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
#include <lib/kprintf.h>

int vmm_freeVirtualPage(uint32_t addr)
{
	uint32_t *pageDir   = (uint32_t *)PD_BASE_ADDR;
	uint32_t *pageTable = 0x0;
	uint32_t  pdIndex   = PD_INDEX(addr);
	uint32_t  ptIndex   = PT_INDEX(addr);
	uint32_t  phys      = 0x0;

	addr &= 0xFFFFF000;

	if ((pageDir[pdIndex] & PAGE_PRESENT) != PAGE_PRESENT)
		return (-1);

	pageTable = (uint32_t *)(PT_BASE_ADDR + (pdIndex * PAGE_SIZE));

	if ((pageTable[ptIndex] & PAGE_PRESENT) != PAGE_PRESENT)
		return (-1);

	phys = pageTable[ptIndex] & 0xFFFFF000;

	if ((pageTable[ptIndex] & PAGE_COW) == PAGE_COW)
		adjustCowCounter(phys, -1);
	else if ((phys >> 12) < (uint32_t)numPages)
		freePage(phys);

	pageTable[ptIndex] = 0x0;

	asm volatile("invlpg (%0)" : : "r"(addr) : "memory");

	return (0);
}
