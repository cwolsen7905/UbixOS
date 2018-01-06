/*-
 * Copyright (c) 2002-2004, 2016, 2018 The UbixOS Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this list of
 * conditions, the following disclaimer and the list of authors.  Redistributions in binary
 * form must reproduce the above copyright notice, this list of conditions, the following
 * disclaimer and the list of authors in the documentation and/or other materials provided
 * with the distribution. Neither the name of the UbixOS Project nor the names of its
 * contributors may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/elf.h>
#include <sys/gdt.h>
#include <ubixos/exec.h>
#include <ubixos/ld.h>
#include <ubixos/kpanic.h>
#include <ubixos/endtask.h>
#include <vmm/vmm.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <lib/string.h>
#include <vfs/file.h>
#include <assert.h>
#include <string.h>

#define ENOEXEC -1

#define AT_NULL         0       /* Terminates the vector. */
#define AT_IGNORE       1       /* Ignored entry. */
#define AT_EXECFD       2       /* File descriptor of program to load. */
#define AT_PHDR         3       /* Program header of program already loaded. */
#define AT_PHENT        4       /* Size of each program header entry. */
#define AT_PHNUM        5       /* Number of program header entries. */
#define AT_PAGESZ       6       /* Page size in bytes. */
#define AT_BASE         7       /* Interpreter's base address. */
#define AT_FLAGS        8       /* Flags (unused for i386). */
#define AT_ENTRY        9       /* Where interpreter should transfer control. */

#define AUXARGS_ENTRY(pos, id, val) {*pos = id;pos++; *pos = val;pos++;}

typedef struct elf_file {
  int preloaded; /* Was file pre-loaded */
  caddr_t address; /* Relocation address */
  Elf32_Dyn *dynamic; /* Symbol table etc. */
  Elf32_Hashelt nbuckets; /* DT_HASH info */
  Elf32_Hashelt nchains;
  const Elf32_Hashelt *buckets;
  const Elf32_Hashelt *chains;
  caddr_t hash;
  caddr_t strtab; /* DT_STRTAB */
  int strsz; /* DT_STRSZ */
  const Elf32_Sym *symtab; /* DT_SYMTAB */
  Elf32_Addr *got; /* DT_PLTGOT */
  const Elf32_Rel *pltrel; /* DT_JMPREL */
  int pltrelsize; /* DT_PLTRELSZ */
  const Elf32_Rela *pltrela; /* DT_JMPREL */
  int pltrelasize; /* DT_PLTRELSZ */
  const Elf32_Rel *rel; /* DT_REL */
  int relsize; /* DT_RELSZ */
  const Elf32_Rela *rela; /* DT_RELA */
  int relasize; /* DT_RELASZ */
  caddr_t modptr;
  const Elf32_Sym *ddbsymtab; /* The symbol table we are using */
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
  Elf32_Addr pcpu_start; /* Pre-relocation pcpu set start. */
  Elf32_Addr pcpu_stop; /* Pre-relocation pcpu set stop. */
  Elf32_Addr pcpu_base; /* Relocated pcpu set address. */
  Elf32_Addr ld_addr; // Entry Point Of Linker (Load It Too)
}*elf_file_t;

static int elf_parse_dynamic(elf_file_t ef);

/*****************************************************************************************

 Function:   execThread(void (*)(void),int,char *);
 Description: This function will create a thread from code in the current memory space

 Notes:

 05/19/04 - This does not work the way I want it to it still makes a copy of kernel space
 so do not use out side of kernel space

 *****************************************************************************************/
uInt32 execThread(void (*tproc)(void), uint32_t stack, char *arg) {
  kTask_t * newProcess = 0x0;
  /* Find A New Thread */
  newProcess = schedNewTask();

  assert(newProcess);
  if (stack < 0x100000)
    kpanic("exec: stack not in valid area: [0x%X]\n", stack);

  /* Set All The Correct Thread Attributes */
  newProcess->tss.back_link = 0x0;
  newProcess->tss.esp0 = 0x0;
  newProcess->tss.ss0 = 0x0;
  newProcess->tss.esp1 = 0x0;
  newProcess->tss.ss1 = 0x0;
  newProcess->tss.esp2 = 0x0;
  newProcess->tss.ss2 = 0x0;
  newProcess->tss.cr3 = (unsigned int) kernelPageDirectory;
  newProcess->tss.eip = (unsigned int) tproc;
  newProcess->tss.eflags = 0x206;
  newProcess->tss.esp = stack;
  newProcess->tss.ebp = stack;
  newProcess->tss.esi = 0x0;
  newProcess->tss.edi = 0x0;

  /* Set these up to be ring 3 tasks */
  /*
   newProcess->tss.es           = 0x30+3;
   newProcess->tss.cs           = 0x28+3;
   newProcess->tss.ss           = 0x30+3;
   newProcess->tss.ds           = 0x30+3;
   newProcess->tss.fs           = 0x30+3;
   newProcess->tss.gs           = 0x30+3;
   */

  newProcess->tss.es = 0x10;
  newProcess->tss.cs = 0x08;
  newProcess->tss.ss = 0x10;
  newProcess->tss.ds = 0x10;
  newProcess->tss.fs = 0x10;
  newProcess->tss.gs = 0x10;

  newProcess->tss.ldt = 0x18;
  newProcess->tss.trace_bitmap = 0x0000;
  newProcess->tss.io_map = 0x8000;
  newProcess->oInfo.vmStart = 0x6400000;

  newProcess->imageFd = 0x0;

  /* Set up default stack for thread here filled with arg list 3 times */
  asm volatile(
      "pusha               \n"
      "movl   %%esp,%%ecx  \n"
      "movl   %1,%%eax     \n"
      "movl   %%eax,%%esp  \n"
      "pushl  %%ebx        \n"
      "pushl  %%ebx        \n"
      "pushl  %%ebx        \n"
      "movl   %%esp,%%eax  \n"
      "movl   %%eax,%1     \n"
      "movl   %%ecx,%%esp  \n"
      "popa                \n"
      :
      : "b" (arg),"m" (newProcess->tss.esp)
  );

  /* Put new thread into the READY state */
  sched_setStatus(newProcess->id, READY);

  /* Return with the new process ID */
  return ((uInt32) newProcess);
}

/*****************************************************************************************

 Function: void execFile(char *file);
 Description: This Function Executes A Kile Into A New VM Space With Out
 Having To Fork
 Notes:

 07/30/02 - I Have Made Some Heavy Changes To This As Well As Fixed A Few
 Memory Leaks The Memory Allocated To Load The Binary Into Is
 Now Unmapped So It Can Be Used Again And Not Held Onto Until
 The Program Exits

 07/30/02 - Now I Have To Make A Better Memory Allocator So We Can Set Up
 The Freshly Allocated Pages With The Correct Permissions

 *****************************************************************************************/
void execFile(char *file, int argc, char **argv, int console) {

  int i = 0x0;
  int x = 0x0;
  uint32_t *tmp = 0x0;

  fileDescriptor *tmpFd = 0x0;
  Elf32_Ehdr *binaryHeader = 0x0;
  elfProgramHeader *programHeader = 0x0;

  /* Get A New Task For This Proccess */
  _current = schedNewTask();
  assert(_current);

  _current->gid = 0x0;
  _current->uid = 0x0;
  _current->term = tty_find(console);

  if (_current->term == 0x0)
    kprintf("Error: invalid console\n");

  /* Set tty ownership */
  _current->term->owner = _current->id;

  /* Now We Must Create A Virtual Space For This Proccess To Run In */
  _current->tss.cr3 = (uInt32) vmm_createVirtualSpace(_current->id);
  kprintf("_current->tss.cr3: 0x%X", _current->tss.cr3);
  /* To Better Load This Application We Will Switch Over To Its VM Space */
  asm volatile(
      "movl %0,%%eax          \n"
      "movl %%eax,%%cr3       \n"
      : : "d" ((uInt32 *)(_current->tss.cr3))
  );
  /* Lets Find The File */
  tmpFd = fopen(file, "r");

  /* If We Dont Find the File Return */
  if (tmpFd == 0x0) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    fclose(tmpFd);
    return;
  }
  if (tmpFd->perms == 0x0) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    fclose(tmpFd);
    return;
  }

  /* Load ELF Header */
  binaryHeader = (Elf32_Ehdr *) kmalloc(sizeof(Elf32_Ehdr));

  //kprintf(">a:%i:0x%X:0x%X<",sizeof(Elf32_Ehdr),binaryHeader,tmpFd);
  fread(binaryHeader, sizeof(Elf32_Ehdr), 1, tmpFd);

  /* Check If App Is A Real Application */
  if ((binaryHeader->e_ident[1] != 'E') && (binaryHeader->e_ident[2] != 'L') && (binaryHeader->e_ident[3] != 'F')) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
  } else if (binaryHeader->e_type != 2) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
  } else if (binaryHeader->e_entry == 0x300000) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
  }

  _current->td.abi = binaryHeader->e_ident[EI_OSABI];

  /* Load The Program Header(s) */
  programHeader = (elfProgramHeader *) kmalloc(sizeof(elfProgramHeader) * binaryHeader->e_phnum);
  fseek(tmpFd, binaryHeader->e_phoff, 0);

  //kprintf(">c:%i:0x%X:0x%X<",sizeof(elfProgramHeader)*binaryHeader->e_phnum,programHeader,tmpFd);
  fread(programHeader, (sizeof(elfProgramHeader) * binaryHeader->e_phnum), 1, tmpFd);
  //kprintf(">d<");

  /* Loop Through The Header And Load Sections Which Need To Be Loaded */
  for (i = 0; i < binaryHeader->e_phnum; i++) {
    if (programHeader[i].phType == 1) {
      /*
       Allocate Memory Im Going To Have To Make This Load Memory With Correct
       Settings so it helps us in the future
       */
      for (x = 0x0; x < (programHeader[i].phMemsz); x += 0x1000) {
        /* Make readonly and read/write !!! */
        if (vmm_remapPage(vmm_findFreePage(_current->id), ((programHeader[i].phVaddr & 0xFFFFF000) + x), PAGE_DEFAULT) == 0x0)
          K_PANIC("Remap Page Failed");

        memset((void *) ((programHeader[i].phVaddr & 0xFFFFF000) + x), 0x0, 0x1000);

      }
      _current->oInfo.vmStart = 0x80000000;
      _current->td.vm_daddr = (u_long) (programHeader[i].phVaddr & 0xFFFFF000);
      /* Now Load Section To Memory */
      fseek(tmpFd, programHeader[i].phOffset, 0);
      fread((void *) programHeader[i].phVaddr, programHeader[i].phFilesz, 1, tmpFd);
      if ((programHeader[i].phFlags & 0x2) != 0x2) {
        kprintf("pH: [0x%X]\n", programHeader[i].phMemsz);
        for (x = 0x0; x < (programHeader[i].phMemsz); x += 0x1000) {
          if ((vmm_setPageAttributes((programHeader[i].phVaddr & 0xFFFFF000) + x, PAGE_PRESENT | PAGE_USER)) != 0x0)
            kpanic("Error: vmm_setPageAttributes failed, File: %s, Line: %i\n", __FILE__, __LINE__);
        }
      }
    }
  }

  /* Set Virtual Memory Start */
  _current->oInfo.vmStart = 0x80000000;
  _current->td.vm_daddr = (u_long) (programHeader[i].phVaddr & 0xFFFFF000);

  /* Set Up Stack Space */
  //MrOlsen (2016-01-14) FIX: is the stack start supposed to be addressable xhcnage x= 1 to x=0
  for (x = 0; x < 100; x++) {
    vmm_remapPage(vmm_findFreePage(_current->id), STACK_ADDR - (x * 0x1000), PAGE_DEFAULT | PAGE_STACK);
  }

  /* Kernel Stack 0x2000 bytes long */
  vmm_remapPage(vmm_findFreePage(_current->id), 0x5BC000, KERNEL_PAGE_DEFAULT | PAGE_STACK);
  vmm_remapPage(vmm_findFreePage(_current->id), 0x5BB000, KERNEL_PAGE_DEFAULT | PAGE_STACK);

  /* Set All The Proper Information For The Task */
  _current->tss.back_link = 0x0;
  _current->tss.esp0 = 0x5BC000;
  _current->tss.ss0 = 0x10;
  _current->tss.esp1 = 0x0;
  _current->tss.ss1 = 0x0;
  _current->tss.esp2 = 0x0;
  _current->tss.ss2 = 0x0;
  _current->tss.eip = (long) binaryHeader->e_entry;
  _current->tss.eflags = 0x206;
  _current->tss.esp = STACK_ADDR - 16;
  _current->tss.ebp = STACK_ADDR;
  _current->tss.esi = 0x0;
  _current->tss.edi = 0x0;

  /* Set these up to be ring 3 tasks */
  _current->tss.es = 0x30 + 3;
  _current->tss.cs = 0x28 + 3;
  _current->tss.ss = 0x30 + 3;
  _current->tss.ds = 0x30 + 3;
  _current->tss.fs = 0x30 + 3;
  _current->tss.gs = 0x50 + 3; //0x30 + 3;

  _current->tss.ldt = 0x18;
  _current->tss.trace_bitmap = 0x0000;
  _current->tss.io_map = 0x8000;

  sched_setStatus(_current->id, READY);

  kfree(binaryHeader);
  kfree(programHeader);
  fclose(tmpFd);

  tmp = (uInt32 *) _current->tss.esp0 - 5;
  tmp[0] = binaryHeader->e_entry;
  tmp[3] = STACK_ADDR - 12;

  tmp = (uint32_t *) _current->tss.esp;

  kprintf("argv: [0x%X]\n", argv);
  //*tmp++ = 0x0; // Stack EIP Return Addr
  //*tmp++ = tmp + 1; // Pointer To AP
  *tmp++ = 0x1; // ARGC
  *tmp++ = 0x100; // ARGV
  *tmp++ = 0x0; // ARGV TERM
  *tmp++ = 0x0; // ENV
  *tmp++ = 0x0; // ENV TERM
  *tmp++ = 0x0; // AUX 1.A
  *tmp++ = 0x0; // AUX 1.B
  *tmp++ = 0x0; // AUX TERM
  *tmp++ = 0x0; // AUX TERM 
  *tmp++ = 0x1; // TERM

  /* Switch Back To The Kernels VM Space */
  asm volatile(
      "movl %0,%%eax          \n"
      "movl %%eax,%%cr3       \n"
      : : "d" ((uInt32 *)(kernelPageDirectory))
  );

  kprintf("execFile Return: %i\n", _current->id);
  /* Finally Return */
  return;
}

/*****************************************************************************************

 Function: void sysExec();
 Description: This Is The System Call To Execute A New Task

 Notes:

 2016-01-19 - Start Of Enhanced Exec

 04-22-03 - It Now Loads Sections Not The Full File

 *****************************************************************************************/

int sys_exec(struct thread *td, char *file, char **argv, char **envp) {
  int i = 0x0;
  int x = 0x0;
  int argc = 0x0;
  uint32_t cr3 = 0x0;

  unsigned int *tmp = 0x0;

  uInt32 seg_size = 0x0;
  uInt32 seg_addr = 0x0;

  char *interp = 0x0;
  //char **argv = 0x0;
  char **argvNew = 0x0;
  char *args = 0x0;

  fileDescriptor *fd = 0x0;

  Elf32_Ehdr *binaryHeader = 0x0;
  Elf32_Phdr *programHeader = 0x0;
  Elf32_Shdr *sectionHeader = 0x0;

  elf_file_t ef = 0x0;

  u_long text_addr = 0, text_size = 0;
  u_long data_addr = 0, data_size = 0;

  struct i386_frame *iFrame = 0x0;

  //Elf_Auxargs *auxargs = 0x0;

  asm("movl %%cr3, %0;" : "=r" (cr3));

  fd = fopen(file, "r");

  /* If the file doesn't exist fail */
  if (fd == 0x0)
    return (-1);

  /* Test If Executable */
  if (fd->perms == 0) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    fclose(fd);
    return (-1);
  }

  /* Set Threads FD to open FD */
  _current->imageFd = fd;

  //! Clean the virtual of COW pages left over from the fork
  //vmm_cleanVirtualSpace( (uint32_t) _current->td.vm_daddr + (_current->td.vm_dsize << PAGE_SHIFT) );
  //MrOlsen 2017-12-15 - FIX! - This should be done before it was causing a lot of problems why did I free space after loading binary????
  vmm_cleanVirtualSpace((uint32_t) 0x8048000);

  /* Load ELF Header */
  if ((binaryHeader = (Elf32_Ehdr *) kmalloc(sizeof(Elf32_Ehdr))) == 0x0)
    K_PANIC("MALLOC FAILED");

  fread(binaryHeader, sizeof(Elf32_Ehdr), 1, fd);
  /* Done Loading ELF Header */

  /* Check If App Is A Real Application */
  if ((binaryHeader->e_ident[1] != 'E') && (binaryHeader->e_ident[2] != 'L') && (binaryHeader->e_ident[3] != 'F')) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(fd);
    return (-1);
  } else if (binaryHeader->e_type != ET_EXEC) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(fd);
    return (-1);
  } else if (binaryHeader->e_entry == 0x300000) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(fd);
    return (-1);
  }

  /* Set Thread ABI */
  td->abi = binaryHeader->e_ident[EI_OSABI];

  /* Load The Program Header(s) */
  if ((programHeader = (Elf32_Phdr *) kmalloc(sizeof(Elf32_Phdr) * binaryHeader->e_phnum)) == 0x0)
    K_PANIC("MALLOC FAILED");

  assert(programHeader);

  fseek(fd, binaryHeader->e_phoff, 0);
  fread(programHeader, (sizeof(elfProgramHeader) * binaryHeader->e_phnum), 1, fd);
  /* Done Loading Program Header(s) */

  /* Load The Section Header(s) */
  if ((sectionHeader = (Elf32_Shdr *) kmalloc(sizeof(Elf32_Shdr) * binaryHeader->e_shnum)) == 0x0)
    K_PANIC("MALLOC FAILED");

  assert(sectionHeader);
  fseek(fd, binaryHeader->e_shoff, 0);
  fread(sectionHeader, sizeof(elfSectionHeader) * binaryHeader->e_shnum, 1, fd);
  /* Done Loading Section Header(s) */

  ef = kmalloc(sizeof(struct elf_file));

  /* Loop Through The Header And Load Sections Which Need To Be Loaded */
  for (i = 0; i < binaryHeader->e_phnum; i++) {
    switch (programHeader[i].p_type) {
      case PT_LOAD:
        if (programHeader[i].p_memsz == 0x0)
          break;

        seg_addr = trunc_page(programHeader[i].p_vaddr);
        seg_size = round_page(programHeader[i].p_memsz + programHeader[i].p_vaddr - seg_addr);

        /*
         Allocate Memory Im Going To Have To Make This Load Memory With Correct
         Settings so it helps us in the future
         */
        for (x = 0x0; x < (round_page(programHeader[i].p_memsz)); x += 0x1000) {
          /* Make readonly and read/write !!! */
          if (vmm_remapPage(vmm_findFreePage(_current->id), ((programHeader[i].p_vaddr & 0xFFFFF000) + x), PAGE_DEFAULT) == 0x0) {
            K_PANIC("Error: Remap Page Failed");
          } /*
           else {
           kprintf("rP[0x%X]", (programHeader[i].phVaddr & 0xFFFFF000) + x);
           } */

          memset((void *) ((programHeader[i].p_vaddr & 0xFFFFF000) + x), 0x0, 0x1000);

        }

        /* Now Load Section To Memory */
        fseek(fd, programHeader[i].p_offset, 0);
        fread((void *) programHeader[i].p_vaddr, programHeader[i].p_filesz, 1, fd);

        if ((programHeader[i].p_flags & 0x2) != 0x2) {
          for (x = 0x0; x < (round_page(programHeader[i].p_memsz)); x += 0x1000) {
            if ((vmm_setPageAttributes((programHeader[i].p_vaddr & 0xFFFFF000) + x, PAGE_PRESENT | PAGE_USER)) != 0x0)
              kpanic("Error: vmm_setPageAttributes failed, File: %s,Line: %i\n", __FILE__, __LINE__);
          }
        }

        if ((programHeader[i].p_flags & PF_X) && text_size < seg_size) {
          kprintf("setting text: 0x%X - 0x%X\n", seg_addr, seg_size);
          text_size = seg_size;
          text_addr = seg_addr;
        } else {
          kprintf("setting data: 0x%X - 0x%X\n", seg_addr, seg_size);
          data_size = seg_size;
          data_addr = seg_addr;
          /*
           _current->td.vm_dsize = seg_size >> PAGE_SHIFT;
           _current->td.vm_daddr = (char *) seg_addr;
           kprintf( "setting daddr: 0x%X, dsiize: 0x%X\n", _current->td.vm_daddr, _current->td.vm_dsize );
           */
        }

        /*
         *  MrOlsen (2016-01-19) NOTE: Note Sure, I should Do This Later
         * Thjis is for stack space
         */
        _current->oInfo.vmStart = ((programHeader[i].p_vaddr & 0xFFFFF000) + 0xA900000);
        break;
      case PT_DYNAMIC:
        //newLoc = (char *)programHeader[i].phVaddr;
        //elfDynamicS = (elfDynamic *) programHeader[i].p_vaddr;
        ef->dynamic = (Elf32_Dyn *) programHeader[i].p_vaddr;
        //fseek( fd, programHeader[i].phOffset, 0 );
        //fread( (void *) programHeader[i].phVaddr, programHeader[i].phFilesz, 1, fd );
        break;
      case PT_INTERP:
        kprintf("Malloc: %i\n", programHeader[i].p_filesz);
        interp = (char *) kmalloc(programHeader[i].p_filesz);
        fseek(fd, programHeader[i].p_offset, 0);
        fread((void *) interp, programHeader[i].p_filesz, 1, fd);
        kprintf("Interp: [%s]\n", interp);
        //ldAddr = ldEnable();
        ef->ld_addr = ldEnable();
        break;
      case PT_GNU_STACK:
        break;
      default:
        break;
    }
  }

  _current->td.vm_tsize = text_size >> PAGE_SHIFT;
  _current->td.vm_taddr = text_addr;
  _current->td.vm_dsize = data_size >> PAGE_SHIFT;
  _current->td.vm_daddr = data_addr;

  kprintf("Done Looping\n");

  elf_parse_dynamic(ef);

  /*
   _current->td.vm_dsize = seg_size >> PAGE_SHIFT;
   _current->td.vm_daddr = (char *) seg_addr;
   */

  //argv = &ap;
  //if (argv[1] != 0x0) {
  if (argv != 0x0) {
//MrOlsen (2016-01-12) FIX: Not sure why argv[0] == 0
    argc = ((int) argv[0] > 0) ? (int) argv[0] : 1;

    kprintf("argc: %i", argc);
    args = (char *) vmm_getFreeVirtualPage(_current->id, 1, VM_TASK);
    kprintf("argc: %i, args 0x%X", argc, args);
    memset(args, 0x0, 0x1000);
    x = 0x0;
    argvNew = (char **) kmalloc(sizeof(char *) * argc);
    for (i = 0x0; i < argc; i++) {
      strcpy(args + x, argv[i + 1]);
      argvNew[i] = args + x;
      x += strlen(argv[i + 1]) + 1;
      //args[x] = '\0';
      //x++;
    }
    argv = argvNew;
  } else {
    argc = 1;
  }

  iFrame = (struct i386_frame *) (_current->tss.esp0 - sizeof(struct i386_frame));
  /*
   iFrameNew = (struct i386_frame *) kmalloc( sizeof(struct i386_frame) );

   memcpy( iFrameNew, iFrame, sizeof(struct i386_frame) );
   */

  //! Adjust iframe
//  iFrame = (struct i386_frame *) (_current->tss.esp0 - sizeof(struct i386_frame));
  //kprintf( "EBP-1(%i): EBP: [0x%X], EIP: [0x%X], ESP: [0x%X]\n", _current->id, iFrame->ebp, iFrame->eip, iFrame->user_esp );
  argc = 1;

  iFrame->ebp = 0x0;      //STACK_ADDR;
  iFrame->eip = binaryHeader->e_entry;
  //iFrame->user_ebp = 0x0;
  iFrame->edx = 0x0;
  //iFrame->user_esp = ((uint32_t) STACK_ADDR) - ((sizeof(uint32_t) * (argc + 8 + 1)) + (sizeof(Elf32_Auxinfo) * 2));
  iFrame->user_esp = ((uint32_t) STACK_ADDR) - (128);      //(sizeof(uint32_t) * (argc + 8 + 1)) + (sizeof(Elf32_Auxinfo) * 2));

  tmp = (void *) iFrame->user_esp; //MrOlsen 2017-11-14 iFrame->user_ebp;

  kprintf("[0x%X][0x%X]", iFrame->user_esp, tmp);

  //memset(tmp,0x0,((sizeof(uint32_t) * (argc + 8 + 1)) + (sizeof(Elf32_Auxinfo) * 2)));
  memset((char *) (STACK_ADDR - 128), 0x0, 128);

  tmp[0] = argc; // ARGC

  /*
   if ( argc == 1 ) {
   *tmp++ = 0x0; //ARGV Pointers
   }
   else {
   for ( i = 0; i < argc; i++ ) {
   *tmp++ = (u_int) argv[i];
   }
   }
   */
  tmp[1] = 0x0; // ARGV
  tmp[2] = 0x0; // ARGV Terminator

  tmp[3] = 0x0; // ENV 
  tmp[4] = 0x0; // ENV Terminator

  tmp[5] = 0x0;
  tmp[6] = 0x0;

  tmp[7] = 0x0; // AUX VECTOR 8 Bytes
  tmp[8] = 0x0; // Terminator

  tmp[9] = 0xDEADBEEF; // End Marker

  kfree(argvNew);

  /* Now That We Relocated The Binary We Can Unmap And Free Header Info */
  kfree(binaryHeader);
  kfree(programHeader);

  /*
   _current->tss.es = 0x30 + 3;
   _current->tss.cs = 0x28 + 3;
   _current->tss.ss = 0x30 + 3;
   _current->tss.ds = 0x30 + 3;
   _current->tss.fs = 0x30 + 3;
   _current->tss.gs = 0x50 + 3; //0x30 + 3;

   _current->tss.ldt = 0x18;
   _current->tss.trace_bitmap = 0x0000;
   _current->tss.io_map = 0x8000;
   */

  /*
   kfree (iFrameNew);

   memAddr = (uint32_t) & (_current->tss);
   ubixGDT[4].descriptor.baseLow = (memAddr & 0xFFFF);
   ubixGDT[4].descriptor.baseMed = ((memAddr >> 16) & 0xFF);
   ubixGDT[4].descriptor.baseHigh = (memAddr >> 24);
   ubixGDT[4].descriptor.access = '\x89';

   ubixGDT[10].descriptor.baseLow = (STACK_ADDR & 0xFFFF);
   ubixGDT[10].descriptor.baseMed = ((STACK_ADDR >> 16) & 0xFF);
   ubixGDT[10].descriptor.baseHigh = (STACK_ADDR >> 24);

   */

  return (0x0);
}

static int elf_parse_dynamic(elf_file_t ef) {
  Elf32_Dyn *dynp;
  int plttype = DT_REL;
  uint32_t *tmp;

  for (dynp = ef->dynamic; dynp->d_tag != 0x0; dynp++) {
    switch (dynp->d_tag) {
      case DT_NEEDED:
        asm("nop");
        break;
      case DT_INIT:
        asm("nop");
        break;
      case DT_FINI:
        asm("nop");
        break;
      case DT_HASH:
        asm("nop");
        /* From src/libexec/rtld-elf/rtld.c */
        const Elf32_Hashelt *hashtab = (const Elf32_Hashelt *) (ef->address + dynp->d_un.d_ptr);
        ef->nbuckets = hashtab[0];
        ef->nchains = hashtab[1];
        ef->buckets = hashtab + 2;
        ef->chains = ef->buckets + ef->nbuckets;
        break;
      case DT_STRTAB:
        ef->strtab = (caddr_t) (ef->address + dynp->d_un.d_ptr);
        break;
      case DT_STRSZ:
        ef->strsz = dynp->d_un.d_val;
        break;
      case DT_SYMTAB:
        ef->symtab = (Elf32_Sym *) (ef->address + dynp->d_un.d_ptr);
        break;
      case DT_SYMENT:
        if (dynp->d_un.d_val != sizeof(Elf32_Sym))
          return (ENOEXEC);
        break;
      case DT_REL:
        ef->rel = (const Elf32_Rel *) (ef->address + dynp->d_un.d_ptr);
        break;
      case DT_RELSZ:
        ef->relsize = dynp->d_un.d_val;
        break;
      case DT_RELENT:
        if (dynp->d_un.d_val != sizeof(Elf32_Rel))
          return (ENOEXEC);
        break;
      case DT_JMPREL:
        ef->pltrel = (const Elf32_Rel *) (ef->address + dynp->d_un.d_ptr);
        break;
      case DT_PLTRELSZ:
        ef->pltrelsize = dynp->d_un.d_val;
        break;
      case DT_RELA:
        ef->rela = (const Elf32_Rela *) (ef->address + dynp->d_un.d_ptr);
        break;
      case DT_RELASZ:
        ef->relasize = dynp->d_un.d_val;
        break;
      case DT_RELAENT:
        if (dynp->d_un.d_val != sizeof(Elf32_Rela))
          return (ENOEXEC);
        break;
      case DT_PLTREL:
        plttype = dynp->d_un.d_val;
        if (plttype != DT_REL && plttype != DT_RELA)
          return (ENOEXEC);
        break;
      case DT_PLTGOT:
        ef->got = (Elf32_Addr *) (ef->address + dynp->d_un.d_ptr);
        tmp = (void *) dynp->d_un.d_ptr; //elfDynamicS[i].dynPtr;
        if (tmp == 0x0)
          kpanic("tmp: NULL\n");
        else
          kprintf("[0x%X]", tmp);
        tmp[2] = (uInt32) ef->ld_addr;
        tmp[1] = (uInt32) ef;
        break;
      default:
        kprintf("t_tag: 0x%X>", dynp->d_tag);
        break;
    }
  }
  ef->pltrela = (const Elf32_Rela *) ef->pltrel;
  ef->pltrel = NULL;
  ef->pltrelasize = ef->pltrelsize;
  ef->pltrelsize = 0;

  ef->ddbsymtab = ef->symtab;
  ef->ddbsymcnt = ef->nchains;
  ef->ddbstrtab = ef->strtab;
  ef->ddbstrcnt = ef->strsz;
  return (0);
}
