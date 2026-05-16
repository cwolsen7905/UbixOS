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

#include <sys/dma_mem.h>
#include <vmm/vmm.h>
#include <vmm/paging.h>
#include <lib/kmalloc.h>
#include <string.h>

/*
 * Allocate one page of physically contiguous, cache-disabled memory.
 * size  — bytes needed; must be > 0 and <= 4096.
 * align — required physical alignment (power of two, 1–4096); ignored on
 *         UbixOS because vmm_findFreePage always returns a page-aligned frame.
 * buf   — filled in on success.
 * Returns 0 on success, -1 on failure.
 */
int
dma_alloc(uint32_t size, uint32_t align, struct dma_buf *buf)
{
	uint32_t phys;

	(void)align;

	if (buf == NULL || size == 0 || size > 0x1000)
		return (-1);

	phys = vmm_findFreePage(sysID);
	if (phys == 0)
		return (-1);

	vmm_remapIOPage(phys, KERNEL_PAGE_DEFAULT | PAGE_CACHE_DISABLED, sysID);

	memset((void *)phys, 0, 0x1000);

	buf->db_vaddr = (void *)phys;
	buf->db_paddr = phys;
	buf->db_size  = size;

	return (0);
}

/*
 * Free a DMA buffer previously returned by dma_alloc().
 */
void
dma_free(struct dma_buf *buf)
{
	if (buf == NULL || buf->db_vaddr == NULL)
		return;

	freePage(buf->db_paddr);
	vmm_unmapPage((uint32_t)buf->db_vaddr, VMM_KEEP);

	buf->db_vaddr = NULL;
	buf->db_paddr = 0;
	buf->db_size  = 0;
}
