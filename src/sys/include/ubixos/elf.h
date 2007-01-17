/*****************************************************************************************
 Copyright (c) 2002-2004 The UbixOS Project
 All rights reserved.

 Redistribution and use in source and binary forms, with or without modification, are
 permitted provided that the following conditions are met:

 Redistributions of source code must retain the above copyright notice, this list of
 conditions, the following disclaimer and the list of authors.  Redistributions in binary
 form must reproduce the above copyright notice, this list of conditions, the following
 disclaimer and the list of authors in the documentation and/or other materials provided
 with the distribution. Neither the name of the UbixOS Project nor the names of its
 contributors may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 $Id$

*****************************************************************************************/

#ifndef _ELF_H
#define _ELF_H

#include <ubixos/types.h>
#include <ubixos/sched.h>

#define elfExecutable 0x002
#define elfLibrary    0x003

#define R_386_NONE      0 /* none    none        */
#define R_386_32        1 /* word32  S + A       */
#define R_386_PC32      2 /* word32  S + A - P   */
#define R_386_GOT32     3 /* word32  G + A - P   */
#define R_386_PLT32     4 /* word32  L + A - P   */
#define R_386_COPY      5 /* none    none        */
#define R_386_GLOB_DAT  6 /* word32  S           */
#define R_386_JMP_SLOT  7 /* word32  S           */
#define R_386_RELATIVE  8 /* word32  B + A       */
#define R_386_GOTOFF    9 /* word32  S + A - GOT */
#define R_386_GOTPC    10 /* word32  GOT + A - P */


/* Elf Types */
#define ET_NONE         0  // No file type
#define ET_REL          1  // Relocatable file
#define ET_EXEC         2  // Executable file
#define ET_DYN          3  // Shared object file
#define ET_CORE         4  // Core file
#define ET_LOPROC  0xff00  // Processor-specific
#define ET_HIPROC  0xffff
/* End Elf Types */

/* Elf Machine Types */
#define EM_NONE       0  // No machine
#define EM_M32        1  // AT&T WE 32100
#define EM_SPARC      2  // SPARC
#define EM_386        3  // Intel 80386
#define EM_68K        4  // Motorola 68000
#define EM_88K        5  // Motorola 88000
#define EM_860        7  // Intel 80860
#define EM_MIPS       8  // MIPS RS3000
/* End Elf Machines Types */

/* Elf Version */
#define EV_NONE          0  // Invalid version
#define EV_CURRENT       1  // Current version
/* End Elf Version */

/* Elf Program Header Types */
#define PT_NULL          0
#define PT_LOAD          1
#define PT_DYNAMIC       2
#define PT_INTERP  	 3
#define PT_NOTE    	 4
#define PT_SHLIB   	 5
#define PT_PHDR    	 6
#define PT_LOOS	   	 0x60000000
#define PT_HIOS	   	 0x6fffffff
#define PT_LOPROC  	 0x70000000
#define PT_HIPROC  	 0x7fffffff
#define PT_GNU_EH_FRAME	 0x6474e550
#define PT_GNU_STACK	 (PT_LOOS + 0x474e551)
#define PT_GNU_RELRO	 (PT_LOOS + 0x474e552) 
#define PT_PAX_FLAGS	 (PT_LOOS + 0x5041580) 

/* End Elf Program Header Types */

typedef struct {
  uInt8  eIdent[16]; /* File identification. */
  uInt16 eType;      /* File type. */
  uInt16 eMachine;   /* Machine architecture. */
  uInt32  eVersion;   /* ELF format version. */
  uInt32  eEntry;     /* Entry point. */
  uInt32  ePhoff;     /* Program header file offset. */
  uInt32  eShoff;     /* Section header file offset. */
  uInt32  eFlags;     /* Architecture-specific flags. */
  uInt16 eEhsize;    /* Size of ELF header in bytes. */
  uInt16 ePhentsize; /* Size of program header entry. */
  uInt16 ePhnum;     /* Number of program header entries. */
  uInt16 eShentsize; /* Size of section header entry. */
  uInt16 eShnum;     /* Number of section header entries. */
  uInt16 eShstrndx;  /* Section name strings section. */
  } elfHeader;

typedef struct {
  uInt32 phType;         /* Entry type. */
  uInt32 phOffset;       /* File offset of contents. */
  uInt32 phVaddr;        /* Virtual address in memory image. */
  uInt32 phPaddr;        /* Physical address (not used). */
  uInt32 phFilesz;       /* Size of contents in file. */
  uInt32 phMemsz;        /* Size of contents in memory. */
  uInt32 phFlags;        /* Access permission flags. */
  uInt32 phAlign;        /* Alignment in memory and file. */
  } elfProgramHeader;

typedef struct {
  uInt32 shName;        /* Section name (index into the section header string table). */
  uInt32 shType;        /* Section type. */
  uInt32 shFlags;       /* Section flags. */
  uInt32 shAddr;        /* Address in memory image. */
  uInt32 shOffset;      /* Offset in file. */
  uInt32 shSize;        /* Size in bytes. */
  uInt32 shLink;        /* Index of a related section. */
  uInt32 shInfo;        /* Depends on section type. */
  uInt32 shAddralign;   /* Alignment in bytes. */
  uInt32 shEntsize;     /* Size of each entry in section. */
  } elfSectionHeader;

typedef struct {
  uInt32 pltOffset;
  uInt32 pltInfo;
  } elfPltInfo;

typedef struct {
  uInt32 dynName;
  uInt32 dynValue;
  uInt32 dynSize;
  uInt32 dynInfo;
  } elfDynSym;

typedef struct {
  uInt32 dynVal;
  uInt32 dynPtr;
  } elfDynamic;

typedef struct {
        int32_t          execfd;
        u_int32_t        phdr;
        u_int32_t        phent;
        u_int32_t        phnum;
        u_int32_t        pagesz;
        u_int32_t        base;
        u_int32_t        flags;
        u_int32_t        entry;
        u_int32_t        trace;
} Elf_Auxargs;

char *elfGetShType(int);
char *elfGetPhType(int);
char *elfGetRelType(int);
int elf_loadfile(kTask_t *p,const char *file,u_int32_t *addr,u_int32_t *entry);

#define ELF32_R_SYM(i)      ((i)>>8)
#define ELF32_R_TYPE(i)     ((unsigned char)(i))
#define ELF32_R_INFO(s, t)  ((s)<<8+(unsigned char)(t))

#endif

/***
 $Log$
 Revision 1.2  2006/12/15 15:43:46  reddawg
 Changes

 Revision 1.1.1.1  2006/06/01 12:46:13  reddawg
 ubix2

 Revision 1.2  2005/10/12 00:13:37  reddawg
 Removed

 Revision 1.1.1.1  2005/09/26 17:23:54  reddawg
 no message

 Revision 1.7  2004/09/11 01:20:08  apwillia
 Clean up 'Unhandled Header' printfs when compiled in linux

 Revision 1.6  2004/06/16 14:04:51  reddawg
 Renamed a typedef

 Revision 1.5  2004/06/14 12:20:54  reddawg
 notes: many bugs repaired and ld works 100% now.

 Revision 1.4  2004/06/12 01:27:26  reddawg
 shared objects: yes we almost fully support shared objects

 Revision 1.3  2004/05/21 15:20:00  reddawg
 Cleaned up


 END
 ***/
