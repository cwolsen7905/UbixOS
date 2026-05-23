/*-
 * Copyright (c) 2002-2018, 2020 The UbixOS Project.
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

#include <ubixos/ld.h>
#include <ubixos/sched.h>
#include <sys/elf.h>
#include <ubixos/kpanic.h>
#include <lib/kmalloc.h>
#include <fs/vfs/vfs.h>
#include <vmm/vmm.h>
#include <string.h>
#include <assert.h>

uint32_t ldEnable(const char *interp)
{
	int i = 0;
	int x = 0;
	char *newLoc = 0x0;
	fileDescriptor_t *ldFd = 0x0;
	Elf_Ehdr *binaryHeader = 0x0;
	Elf_Phdr *programHeader = 0x0;
	uint32_t entry;

	/*
	 * PT_INTERP paths are POSIX-absolute (e.g. /lib/ld-musl-i386.so.1).
	 * fopen() now resolves POSIX paths via longest-prefix mount matching,
	 * so pass the interp path directly.
	 */
	ldFd = fopen(interp, "rb");
	if (ldFd == 0x0) {
		ldFd = fopen("/libexec/ld.so", "rb");
		if (ldFd == 0x0)
			return (0x0);
	}

	kern_fseek(ldFd, 0x0, 0x0);
	binaryHeader = (Elf32_Ehdr *)kmalloc(sizeof(Elf32_Ehdr));
	assert(binaryHeader);
	fread(binaryHeader, sizeof(Elf32_Ehdr), 1, ldFd);

	programHeader = (Elf_Phdr *)kmalloc(sizeof(Elf_Phdr) * binaryHeader->e_phnum);
	assert(programHeader);
	kern_fseek(ldFd, binaryHeader->e_phoff, 0);
	fread(programHeader, sizeof(*programHeader), binaryHeader->e_phnum, ldFd);

	for (i = 0; i < binaryHeader->e_phnum; i++) {
		if (programHeader[i].p_type != PT_LOAD)
			continue;
		newLoc = (char *)(programHeader[i].p_vaddr + LD_START);
		for (x = 0; x < (int)programHeader[i].p_memsz; x += 0x1000) {
			uint32_t va = (programHeader[i].p_vaddr & 0xFFFFF000) + x + LD_START;
			if (vmm_remapPage(vmm_findFreePage(_current->id), va, PAGE_DEFAULT, _current->id, 0) == 0x0)
				K_PANIC("vmmRemapPage: ld");
			memset((void *)va, 0x0, 0x1000);
		}
		kern_fseek(ldFd, programHeader[i].p_offset, 0x0);
		fread(newLoc, programHeader[i].p_filesz, 1, ldFd);
	}

	entry = binaryHeader->e_entry + LD_START;

	kfree(programHeader);
	kfree(binaryHeader);
	fclose(ldFd);

	return (entry);
}
