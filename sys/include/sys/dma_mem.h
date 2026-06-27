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

#ifndef _SYS_DMA_MEM_H
#define _SYS_DMA_MEM_H

#include <sys/types.h>

/*
 * Physically contiguous, cache-disabled memory for hardware DMA descriptors.
 * On UbixOS kernel pages are identity-mapped, so db_vaddr == db_paddr.
 * Maximum allocation size is one page (4096 bytes).
 */
struct dma_buf {
	void		*db_vaddr;	/* kernel virtual address */
	u_int32_t	 db_paddr;	/* physical address */
	u_int32_t	 db_size;	/* allocation size in bytes */
};

int	 dma_alloc(u_int32_t size, u_int32_t align, struct dma_buf *buf);
void	 dma_free(struct dma_buf *buf);

/* Convert a kernel virtual address to its physical address. */
static inline u_int32_t
dma_paddr(void *vaddr)
{
	return ((u_int32_t)(uintptr_t)vaddr);
}

#endif /* _SYS_DMA_MEM_H */
