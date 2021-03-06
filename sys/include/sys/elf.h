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

#ifndef _SYS_ELF_H_
#define _SYS_ELF_H_ 1

#define __i386__ 1

#include <sys/types.h>
#include <i386/elf.h>
#include <sys/elf32.h>
#include <sys/elf64.h>

typedef struct elf_file {
  int preloaded; /* Was file pre-loaded */
  caddr_t address; /* Relocation address */
  Elf_Dyn *dynamic; /* Symbol table etc. */
  Elf_Hashelt nbuckets; /* DT_HASH info */
  Elf_Hashelt nchains;
  const Elf_Hashelt *buckets;
  const Elf_Hashelt *chains;
  caddr_t hash;
  caddr_t strtab; /* DT_STRTAB */
  int strsz; /* DT_STRSZ */
  const Elf_Sym *symtab; /* DT_SYMTAB */
  Elf_Addr *got; /* DT_PLTGOT */
  const Elf_Rel *pltrel; /* DT_JMPREL */
  int pltrelsize; /* DT_PLTRELSZ */
  const Elf_Rela *pltrela; /* DT_JMPREL */
  int pltrelasize; /* DT_PLTRELSZ */
  const Elf_Rel *rel; /* DT_REL */
  int relsize; /* DT_RELSZ */
  const Elf_Rela *rela; /* DT_RELA */
  int relasize; /* DT_RELASZ */
  caddr_t modptr;
  const Elf_Sym *ddbsymtab; /* The symbol table we are using */
  long ddbsymcnt; /* Number of symbols */
  caddr_t ddbstrtab; /* String table */
  long ddbstrcnt; /* number of bytes in string table */
  caddr_t symbase; /* malloc'ed symbold base */
  caddr_t strbase; /* malloc'ed string base */
  caddr_t ctftab; /* CTF table */
  long ctfcnt; /* number of bytes in CTF table */
  caddr_t ctfoff; /* CTF offset table */
  caddr_t typoff; /* Type offset table */
  long typlen; /* Number of type entries. */
  Elf_Addr pcpu_start; /* Pre-relocation pcpu set start. */
  Elf_Addr pcpu_stop; /* Pre-relocation pcpu set stop. */
  Elf_Addr pcpu_base; /* Relocated pcpu set address. */
} *elf_file_t;


#include <ubixos/ubthread.h>

#define elfExecutable 0x002
#define elfLibrary    0x003

char *elfGetShType( int );
char *elfGetPhType( int );
char *elfGetRelType( int );

int elf_load_file( kTask_t *p, const char *file, uint32_t *addr, uint32_t *entry );

#endif /* END _SYS_ELF_H */
