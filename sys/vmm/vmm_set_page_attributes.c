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
#include <ubixos/kpanic.h>

/************************************************************************

 Function: void vmm_set_page_attributes(u_int32_t pageAddr,int attributes;
 Description: This Function Will Set The Page Attributes Such As
 A Read Only Page, Stack Page, COW Page, ETC.
 Notes:

 ************************************************************************/
int vmm_set_page_attributes(uintptr_t mem_addr, u_int32_t attributes)
{
	u_int32_t directory_index = 0, table_index = 0;
	u_int32_t *page_table = NULL;

	/* Calculate The Page Directory Index */
	directory_index = (mem_addr >> 22);

	/* Calculate The Page Table Index */
	table_index = ((mem_addr >> 12) & 0x3FF);

	/* Set Table Pointer */
	page_table = (u_int32_t *)(PT_BASE_ADDR + (0x1000 * directory_index));
	if (page_table == NULL)
	{
		kpanic("Error: page_table == NULL, File: %s, Line: %i\n", __FILE__, __LINE__);
	}

	/* Set Attribute If Page Is Mapped */
	if (page_table[table_index] != 0)
	{
		page_table[table_index] = ((page_table[table_index] & 0xFFFFF000) | attributes);
	}

	/* Reload The Page Table; */
	asm volatile("push %eax     \n"
	             "movl %cr3,%eax\n"
	             "movl %eax,%cr3\n"
	             "pop  %eax     \n");
	/* Return */
	return 0;
}
