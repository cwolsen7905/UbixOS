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

#include <ubixos/exec.h>
#include <ubixos/elf.h>
#include <ubixos/ld.h>
#include <ubixos/kpanic.h>
#include <ubixos/endtask.h>
#include <vmm/vmm.h>
#include <lib/kmalloc.h>
#include <lib/kprintf.h>
#include <lib/string.h>
#include <assert.h>

/* WHERE SHOULD THE STACK BE? */
#define STACK_ADDR 0x80000000 //0xC800000

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

//#define AUXARGS_ENTRY(pos, id, val) {memcpy((void *)pos++,(void *)id,sizeof(long)); memcpy((void *)pos++,(void *)val,sizeof(long));}
#define AUXARGS_ENTRY(pos, id, val) {*pos = id;pos++; *pos = val;pos++;}



/*****************************************************************************************

 Function:   execThread(void (*)(void),int,char *);
 Description: This function will create a thread from code in the current memory space

 Notes:
 
 05/19/04 - This does not work the way I want it to it still makes a copy of kernel space
            so do not use out side of kernel space

*****************************************************************************************/
uInt32 execThread(void (* tproc)(void),uInt32 stack,char *arg) {
  kTask_t * newProcess = 0x0;
  /* Find A New Thread */
  newProcess = schedNewTask();
  assert(newProcess); 
  if (stack < 0x100000)
    kpanic("exec: stack not in valid area: [0x%X]\n",stack);

  /* Set All The Correct Thread Attributes */
  newProcess->tss.back_link    = 0x0;
  newProcess->tss.esp0         = 0x0;
  newProcess->tss.ss0          = 0x0;
  newProcess->tss.esp1         = 0x0;
  newProcess->tss.ss1          = 0x0;
  newProcess->tss.esp2         = 0x0;
  newProcess->tss.ss2          = 0x0;
  newProcess->tss.cr3          = (unsigned int)kernelPageDirectory;
  newProcess->tss.eip          = (unsigned int)tproc;
  newProcess->tss.eflags       = 0x206;
  newProcess->tss.esp          = stack;
  newProcess->tss.ebp          = stack;
  newProcess->tss.esi          = 0x0;
  newProcess->tss.edi          = 0x0;

  /* Set these up to be ring 3 tasks */
  /*
  newProcess->tss.es           = 0x30+3;
  newProcess->tss.cs           = 0x28+3;
  newProcess->tss.ss           = 0x30+3;
  newProcess->tss.ds           = 0x30+3;
  newProcess->tss.fs           = 0x30+3;
  newProcess->tss.gs           = 0x30+3;
  */

  newProcess->tss.es           = 0x10;
  newProcess->tss.cs           = 0x08;
  newProcess->tss.ss           = 0x10;
  newProcess->tss.ds           = 0x10;
  newProcess->tss.fs           = 0x10;
  newProcess->tss.gs           = 0x10;

  newProcess->tss.ldt          = 0x18;
  newProcess->tss.trace_bitmap = 0x0000;
  newProcess->tss.io_map       = 0x8000;
  newProcess->oInfo.vmStart    = 0x6400000;
  
  newProcess->imageFd          = 0x0;

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
  sched_setStatus(newProcess->id,READY);

  /* Return with the new process ID */
  return((uInt32)newProcess);
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
void execFile(char *file,int argc,char **argv,int console) {

  int        i         = 0x0;
  int        x         = 0x0;
  u_int32_t *tmp       = 0x0;

  fileDescriptor   *tmpFd         = 0x0;
  elfHeader        *binaryHeader  = 0x0;
  elfProgramHeader *programHeader = 0x0;

  /* Get A New Task For This Proccess */
  _current = schedNewTask();
  assert(_current);
  _current->gid  = 0x0;
  _current->uid  = 0x0;
  _current->term = tty_find(console);
  if (_current->term == 0x0)
    kprintf("Error: invalid console\n");

  /* Set tty ownership */
  _current->term->owner = _current->id;

  /* Now We Must Create A Virtual Space For This Proccess To Run In */
  _current->tss.cr3 = (uInt32)vmmCreateVirtualSpace(_current->id);

  /* To Better Load This Application We Will Switch Over To Its VM Space */
  asm volatile(
    "movl %0,%%eax          \n"
    "movl %%eax,%%cr3       \n"
    : : "d" ((uInt32 *)(_current->tss.cr3))
    );

  /* Lets Find The File */
  tmpFd = fopen(file,"r");

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
  binaryHeader = (elfHeader *)kmalloc(sizeof(elfHeader));


  //kprintf(">a:%i:0x%X:0x%X<",sizeof(elfHeader),binaryHeader,tmpFd);
  fread(binaryHeader,sizeof(elfHeader),1,tmpFd);


  /* Check If App Is A Real Application */
  if ((binaryHeader->eIdent[1] != 'E') && (binaryHeader->eIdent[2] != 'L') && (binaryHeader->eIdent[3] != 'F')) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
    }
  else if (binaryHeader->eType != 2) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
    }
  else if (binaryHeader->eEntry == 0x300000) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
    }

  /* Load The Program Header(s) */
  programHeader = (elfProgramHeader *)kmalloc(sizeof(elfProgramHeader)*binaryHeader->ePhnum);
  fseek(tmpFd,binaryHeader->ePhoff,0);

  //kprintf(">c:%i:0x%X:0x%X<",sizeof(elfProgramHeader)*binaryHeader->ePhnum,programHeader,tmpFd);
  fread(programHeader,(sizeof(elfProgramHeader)*binaryHeader->ePhnum),1,tmpFd);
  //kprintf(">d<");

  /* Loop Through The Header And Load Sections Which Need To Be Loaded */
  for (i=0;i<binaryHeader->ePhnum;i++) {
    if (programHeader[i].phType == 1) {
      /*
      Allocate Memory Im Going To Have To Make This Load Memory With Correct
      Settings so it helps us in the future
      */
      for (x = 0x0;x < (programHeader[i].phMemsz);x += 0x1000) {
        /* Make readonly and read/write !!! */
        if (vmm_remapPage(vmmFindFreePage(_current->id),((programHeader[i].phVaddr & 0xFFFFF000) + x),PAGE_DEFAULT) == 0x0)
          K_PANIC("Remap Page Failed");

        memset((void *)((programHeader[i].phVaddr & 0xFFFFF000) + x),0x0,0x1000);
        }
      _current->oInfo.vmStart = 0x80000000;
      _current->td.vm_daddr = (char *)(programHeader[i].phVaddr & 0xFFFFF000);
      /* Now Load Section To Memory */
      fseek(tmpFd,programHeader[i].phOffset,0);
      fread((void *)programHeader[i].phVaddr,programHeader[i].phFilesz,1,tmpFd);
      if ((programHeader[i].phFlags & 0x2) != 0x2) {
        #ifdef DEBUG
        kprintf("pH: [0x%X]\n",programHeader[i].phMemsz);
        #endif
        for (x = 0x0;x < (programHeader[i].phMemsz);x += 0x1000) {
          if ((vmm_setPageAttributes((programHeader[i].phVaddr & 0xFFFFF000) + x,PAGE_PRESENT | PAGE_USER)) != 0x0)
	    kpanic("Error: vmm_setPageAttributes failed, File: %s, Line: %i\n",__FILE__,__LINE__);
	  }
        }
      }
    }

  /* Set Virtual Memory Start */
  _current->oInfo.vmStart = 0x80000000;
  _current->td.vm_daddr = (char *)(programHeader[i].phVaddr & 0xFFFFF000);

  /* Set Up Stack Space */
  for (x = 1;x < 100;x++) {
    vmm_remapPage(vmmFindFreePage(_current->id),STACK_ADDR - (x * 0x1000),PAGE_DEFAULT | PAGE_STACK);
    }

  /* Kernel Stack 0x2000 bytes long */
  vmm_remapPage(vmmFindFreePage(_current->id),0x5BC000,KERNEL_PAGE_DEFAULT | PAGE_STACK);
  vmm_remapPage(vmmFindFreePage(_current->id),0x5BB000,KERNEL_PAGE_DEFAULT | PAGE_STACK);

  /* Set All The Proper Information For The Task */
  _current->tss.back_link    = 0x0;
  _current->tss.esp0         = 0x5BC000;
  _current->tss.ss0          = 0x10;
  _current->tss.esp1         = 0x0;
  _current->tss.ss1          = 0x0;
  _current->tss.esp2         = 0x0;
  _current->tss.ss2          = 0x0;
  _current->tss.eip          = (long)binaryHeader->eEntry;
  _current->tss.eflags       = 0x206;
  _current->tss.esp          = STACK_ADDR - 12;
  _current->tss.ebp          = STACK_ADDR;
  _current->tss.esi          = 0x0;
  _current->tss.edi          = 0x0;

  /* Set these up to be ring 3 tasks */
  _current->tss.es           = 0x30+3;
  _current->tss.cs           = 0x28+3;
  _current->tss.ss           = 0x30+3;
  _current->tss.ds           = 0x30+3;
  _current->tss.fs           = 0x30+3;
  _current->tss.gs           = 0x30+3;

  _current->tss.ldt          = 0x18;
  _current->tss.trace_bitmap = 0x0000;
  _current->tss.io_map       = 0x8000;

  sched_setStatus(_current->id,READY);

  kfree(binaryHeader);
  kfree(programHeader);
  fclose(tmpFd);

  tmp = (uInt32 *)_current->tss.esp0 - 5;
  tmp[0] = binaryHeader->eEntry;
  tmp[3] = STACK_ADDR - 12;

  tmp = (uInt32 *)STACK_ADDR - 2;
  /*
  if (_current->id > 4)
  kprintf("argv[0]: [%s]\n",argv[0]);
  kprintf("argv: [0x%X]\n",argv);
  */
  tmp[0] = (u_int32_t)argv;
  tmp[1] = (u_int32_t)argv;


  /* Switch Back To The Kernels VM Space */
  asm volatile(
    "movl %0,%%eax          \n"
    "movl %%eax,%%cr3       \n"
    : : "d" ((uInt32 *)(kernelPageDirectory))
    );

  /* Finally Return */
  return;
  }

/*****************************************************************************************

 Function: void sysExec();
 Description: This Is The System Call To Execute A New Task

 Notes:
 04-22-03 - It Now Loads Sections Not The Full File

*****************************************************************************************/
void sysExec(char *file,char *ap) {
  int      i        = 0x0;
  int      x        = 0x0;
  int      argc     = 0x0;
  uInt32  *tmp      = 0x0;
  uInt32   ldAddr   = 0x0;
  uInt32   seg_size = 0x0;
  uInt32   seg_addr = 0x0;
  char    *interp   = 0x0;
  char   **argv     = 0x0;
  char   **argvNew  = 0x0;
  char    *args     = 0x0;

  fileDescriptor    *tmpFd         = 0x0;
  elfHeader         *binaryHeader  = 0x0;
  elfProgramHeader  *programHeader = 0x0;
  elfSectionHeader  *sectionHeader = 0x0;
  elfDynamic        *elfDynamicS   = 0x0;
  struct i386_frame *iFrame        = 0x0;

  tmpFd = fopen(file,"r");
  _current->imageFd = tmpFd;
  /* If We Dont Find the File Return */
  if (tmpFd == 0x0) {
    return;
    }
  if (tmpFd->perms == 0) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    fclose(tmpFd);
    return;
    }

  /* Load ELF Header */

  if ((binaryHeader = (elfHeader *)kmalloc(sizeof(elfHeader))) == 0x0) 
    endTask(_current->id);
    fread(binaryHeader,sizeof(elfHeader),1,tmpFd);
    /* Set sectionHeader To Point To Loaded Binary To We Can Gather Info */

  /* Check If App Is A Real Application */
  if ((binaryHeader->eIdent[1] != 'E') && (binaryHeader->eIdent[2] != 'L') && (binaryHeader->eIdent[3] != 'F')) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);

    return;
    }
  else if (binaryHeader->eType != 2) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
    }
  else if (binaryHeader->eEntry == 0x300000) {
    kprintf("Exec Format Error: Binary File Not Executable.\n");
    kfree(binaryHeader);
    fclose(tmpFd);
    return;
    }

  /* Load The Program Header(s) */
  if ((programHeader = (elfProgramHeader *)kmalloc(sizeof(elfProgramHeader)*binaryHeader->ePhnum)) == 0x0)
    endTask(_current->id);

  assert(programHeader);
  fseek(tmpFd,binaryHeader->ePhoff,0);
  fread(programHeader,(sizeof(elfProgramHeader)*binaryHeader->ePhnum),1,tmpFd);

  if ((sectionHeader = (elfSectionHeader *)kmalloc(sizeof(elfSectionHeader)*binaryHeader->eShnum)) == 0x0)
    endTask(_current->id);

  assert(sectionHeader);
  fseek(tmpFd,binaryHeader->eShoff,0);
  fread(sectionHeader,sizeof(elfSectionHeader)*binaryHeader->eShnum,1,tmpFd);

  /* Loop Through The Header And Load Sections Which Need To Be Loaded */
  for (i=0;i<binaryHeader->ePhnum;i++) {
    switch (programHeader[i].phType) {
      case PT_LOAD:
        seg_addr = trunc_page(programHeader[i].phVaddr);
        seg_size = round_page(programHeader[i].phMemsz + programHeader[i].phVaddr - seg_addr);

        /*
        Allocate Memory Im Going To Have To Make This Load Memory With Correct
        Settings so it helps us in the future
        */
        for (x = 0x0;x < (programHeader[i].phMemsz);x += 0x1000) {
          /* Make readonly and read/write !!! */
          if (vmm_remapPage(vmmFindFreePage(_current->id),((programHeader[i].phVaddr & 0xFFFFF000) + x),PAGE_DEFAULT) == 0x0)
            K_PANIC("Error: Remap Page Failed");
          memset((void *)((programHeader[i].phVaddr & 0xFFFFF000) + x),0x0,0x1000);
          }

        /* Now Load Section To Memory */
        fseek(tmpFd,programHeader[i].phOffset,0);
        fread((void *)programHeader[i].phVaddr,programHeader[i].phFilesz,1,tmpFd);
        if ((programHeader[i].phFlags & 0x2) != 0x2) {
          for (x = 0x0;x < (programHeader[i].phMemsz);x += 0x1000) {
            if ((vmm_setPageAttributes((programHeader[i].phVaddr & 0xFFFFF000) + x,PAGE_PRESENT | PAGE_USER)) != 0x0)
	      kpanic("Error: vmm_setPageAttributes failed, File: %s,Line: %i\n",__FILE__,__LINE__);
	    }
          }
        #ifdef DEBUG
        kprintf("setting daddr\n");
        #endif
        if (binaryHeader->eEntry >= programHeader[i].phVaddr && binaryHeader->eEntry < (programHeader[i].phVaddr + programHeader[i].phMemsz)) {
          /* We're suposed to do something here? */
          }
        else {
          _current->td.vm_dsize = seg_size >> PAGE_SHIFT;
          _current->td.vm_daddr = (char *)seg_addr;
          }

        _current->oInfo.vmStart = ((programHeader[i].phVaddr & 0xFFFFF000) + 0xA900000);
        break;
      case PT_DYNAMIC:
        //newLoc = (char *)programHeader[i].phVaddr;
        elfDynamicS = (elfDynamic *)programHeader[i].phVaddr;
        fseek(tmpFd,programHeader[i].phOffset,0);
        fread((void *)programHeader[i].phVaddr,programHeader[i].phFilesz,1,tmpFd);
        break;
      case PT_INTERP:
        interp = (char *)kmalloc(programHeader[i].phFilesz);
        fseek(tmpFd,programHeader[i].phOffset,0);
        fread((void *)interp,programHeader[i].phFilesz,1,tmpFd);
        #ifdef DEBUG
        kprintf("Interp: [%s]\n",interp);
        #endif
        ldAddr = ldEnable();
        break;
      default:
        break;
      }
    }

  /* What is this doing? 11/23/06 */
  if (elfDynamicS != 0x0) {
    for (i=0;i<12;i++) {
      if (elfDynamicS[i].dynVal == 0x3) {
        tmp = (uInt32 *)elfDynamicS[i].dynPtr;
        if (tmp == 0x0)
          kpanic("tmp: NULL\n");
        tmp[2] = (uInt32)ldAddr;
        tmp[1] = (uInt32)tmpFd;
        break;
        }
/*
      else {
        kprintf("dyn_val: %i",elfDynamicS[i].dynVal);
        }
*/
      }
    }

  _current->td.vm_dsize = seg_size >> PAGE_SHIFT;
  _current->td.vm_daddr = (char *)seg_addr;

  argv = ap;

  if (argv[1] != 0x0) {
    argc = argv[0];
    args = (char *)vmmGetFreeVirtualPage(_current->id,1,VM_TASK);
    //! do we need this?
    memset(args,0x0,0x1000);
    x = 0x0;
    argvNew = (char **)kmalloc(sizeof(char *) * argc);
    for (i = 0x0;i < argc;i++) {
      strcpy(args + x,argv[i + 1]);
      argvNew[i] = args + x;
      x += strlen(argv[i + 1]) + 1;
      //args[x] = '\0';
      //x++;
      }
    argv = argvNew;
    }

  //! Clean the virtual of COW pages left over from the fork
  vmm_cleanVirtualSpace(_current->td.vm_daddr + (_current->td.vm_dsize << PAGE_SIZE));


  //! Adjust iframe
  iFrame = _current->tss.esp0 - sizeof(struct i386_frame);
  iFrame->ebp = STACK_ADDR;
  iFrame->eip = binaryHeader->eEntry;
  iFrame->user_esp = STACK_ADDR - 12;

  //if (_current->id > 3) {

    iFrame->user_esp = ((u_int32_t)STACK_ADDR) - (sizeof(u_int32_t) * (argc + 3));
    tmp = iFrame->user_esp;

    //! build argc and argv[]
    tmp[0] = argc;
    for (i = 0;i < argc;i++) {
      tmp[i + 1] = argv[i];
      }
    tmp[argc + 1] = 0x0;
    tmp[argc + 2] = 0x1;
    //}
  //else {
    //tmp = (u_int32_t *)STACK_ADDR - 2;
    //tmp[0] = 0x1;
    //tmp[1] = 0x0;
    //tmp[1] = (u_int32_t)argv;
    //}
  kfree(argvNew);
 /* Now That We Relocated The Binary We Can Unmap And Free Header Info */
  kfree(binaryHeader);
  kfree(programHeader);

  return;
  }

/*!
 * \brief New exec...
 *
 */
void sys_exec(char *file,char *ap) {
  int                 error         = 0x0;
  int                 i             = 0x0;
  int                 x             = 0x0;
  int                 argc          = 0x0;
  u_int32_t          *tmp           = 0x0;
  u_int32_t           seg_size      = 0x0;
  u_int32_t           seg_addr      = 0x0;
  u_int32_t           addr          = 0x0;
  u_int32_t           eip           = 0x0;
  u_int32_t           proghdr       = 0x0;
  char               *args          = 0x0;
  char               *interp        = 0x0;
  char              **argv          = 0x0;
  char              **argvNew       = 0x0;
  elfHeader          *binaryHeader  = 0x0;
  elfProgramHeader   *programHeader = 0x0;
  struct i386_frame  *iFrame        = 0x0;
  Elf_Auxargs        *auxargs       = 0x0;

  _current->imageFd = fopen(file,"r");
  if (_current->imageFd == 0x0)
    return(-1);

  /* Load the ELF header */
  if ((binaryHeader = (elfHeader *)kmalloc(sizeof(elfHeader))) == 0x0) 
    K_PANIC("malloc failed!");
  fread(binaryHeader,sizeof(elfHeader),1,_current->imageFd);

  /* Check If App Is A Real Application */
  if (((binaryHeader->eIdent[1] != 'E') && (binaryHeader->eIdent[2] != 'L') && (binaryHeader->eIdent[3] != 'F')) || (binaryHeader->eType != ET_EXEC)) {
    kfree(binaryHeader);
    fclose(_current->imageFd);
    return(-1);
    }

  /* Load The Program Header(s) */
  if ((programHeader = (elfProgramHeader *)kmalloc(sizeof(elfProgramHeader)*binaryHeader->ePhnum)) == 0x0)
    K_PANIC("malloc failed!");
  fseek(_current->imageFd,binaryHeader->ePhoff,0);
  fread(programHeader,(sizeof(elfProgramHeader)*binaryHeader->ePhnum),1,_current->imageFd);

  /* Loop Through The Header And Load Sections Which Need To Be Loaded */
  for (i = 0x0;i < binaryHeader->ePhnum;i++) {
    switch (programHeader[i].phType) {
      case PT_LOAD:
        seg_addr = trunc_page(programHeader[i].phVaddr);
        seg_size = round_page(programHeader[i].phMemsz + programHeader[i].phVaddr - seg_addr);

        /*
        Allocate Memory Im Going To Have To Make This Load Memory With Correct
        Settings so it helps us in the future
        */
        for (x = 0x0;x < (programHeader[i].phMemsz);x += 0x1000) {
          /* Make readonly and read/write !!! */
          if (vmm_remapPage(vmmFindFreePage(_current->id),((programHeader[i].phVaddr & 0xFFFFF000) + x),PAGE_DEFAULT) == 0x0)
            K_PANIC("Error: Remap Page Failed");
          memset((void *)((programHeader[i].phVaddr & 0xFFFFF000) + x),0x0,0x1000);
          }

        /* Now Load Section To Memory */
        fseek(_current->imageFd,programHeader[i].phOffset,0);
        fread((void *)programHeader[i].phVaddr,programHeader[i].phFilesz,1,_current->imageFd);
        if ((programHeader[i].phFlags & 0x2) != 0x2) {
          for (x = 0x0;x < (programHeader[i].phMemsz);x += 0x1000) {
            if ((vmm_setPageAttributes((programHeader[i].phVaddr & 0xFFFFF000) + x,PAGE_PRESENT | PAGE_USER)) != 0x0)
              K_PANIC("vmm_setPageAttributes failed");
            }
          }
        if (binaryHeader->eEntry >= programHeader[i].phVaddr && binaryHeader->eEntry < (programHeader[i].phVaddr + programHeader[i].phMemsz)) {
          /* We're suposed to do something here? */
          }
        else {
          _current->td.vm_dsize = seg_size >> PAGE_SHIFT;
          _current->td.vm_daddr = (char *)seg_addr;
          }

        _current->oInfo.vmStart = ((programHeader[i].phVaddr & 0xFFFFF000) + 0xA900000);
        break;
      case PT_INTERP:
        interp = (char *)kmalloc(programHeader[i].phFilesz);
        if (interp == 0x0)
          K_PANIC("malloc failed");

        fseek(_current->imageFd,programHeader[i].phOffset,0);
        fread((void *)interp,programHeader[i].phFilesz,1,_current->imageFd);
        #ifdef DEBUG
        kprintf("Interp: [%s]\n",interp);
        #endif
        //ldAddr = ldEnable();
        break;
      case PT_PHDR:
        proghdr = programHeader[i].phVaddr; 
        break;
      default:
        break;
      }
    }

  addr = LD_START;


  if (interp != 0x0) {
    //kprintf("TEST");
    elf_loadfile(_current,interp,&addr,&eip);
    }
  //kprintf("[0x%X][0x%X]\n",eip,addr);

  _current->td.vm_dsize = seg_size >> PAGE_SHIFT;
  _current->td.vm_daddr = (char *)seg_addr;

  //! copy in arg strings
  argv = ap;

  if (argv[1] != 0x0) {
    argc = argv[0];
    args = (char *)vmmGetFreeVirtualPage(_current->id,1,VM_TASK);
    memset(args,0x0,0x1000);
    x = 0x0;
    argvNew = (char **)kmalloc(sizeof(char *) * argc);
    for (i = 0x0;i < argc;i++) {
      strcpy(args + x,argv[i + 1]);
      argvNew[i] = args + x;
      x += strlen(argv[i + 1]) + 1;
      }
    argv = argvNew;
    }

  //! Clean the virtual of COW pages left over from the fork
  vmm_cleanVirtualSpace(_current->td.vm_daddr + (_current->td.vm_dsize << PAGE_SIZE));


  //! Adjust iframe
  iFrame = _current->tss.esp0 - sizeof(struct i386_frame);
  iFrame->ebp = STACK_ADDR;
  iFrame->eip = eip;

  //if (_current->id > 3) {

  iFrame->user_esp = ((u_int32_t)STACK_ADDR) - (sizeof(u_int32_t) * (argc + 4 + (sizeof(Elf_Auxargs) * 2)));
  #ifdef DEBUG
  kprintf("\n\n\nuser_esp: [0x%X]\n",iFrame->user_esp);
  #endif
  tmp = iFrame->user_esp;

  //! build argc and argv[]
  tmp[0] = argc;
  for (i = 0;i < argc;i++) {
    tmp[i + 1] = argv[i];
    }
  //! Build ENV
  args = (char *)vmmGetFreeVirtualPage(_current->id,1,VM_TASK);
  memset(args,0x0,0x1000);

  strcpy(args,"LIBRARY_PATH=/lib");
  tmp[argc + 2] = args;
  #ifdef DEBUG
  kprintf("env: [0x%X][0x%X]\n",(uInt32)tmp + argc + 2,tmp[argc + 2]);
  #endif
  tmp[argc + 3] = 0x0;
  #ifdef DEBUG
  kprintf("env: [0x%X][0x%X]\n",(uInt32)tmp + argc + 2,tmp[argc + 2]);
  #endif

  tmp = iFrame->user_esp;
  tmp += argc + 4;


  auxargs->execfd = -1;
  auxargs->phdr   = proghdr;
  auxargs->phent  = binaryHeader->ePhentsize;
  auxargs->phnum  = binaryHeader->ePhnum;
  auxargs->pagesz = PAGE_SIZE;
  auxargs->base   = addr;
  auxargs->flags  = 0x0;
  auxargs->entry  = binaryHeader->eEntry;
  auxargs->trace  = 0x0;

  AUXARGS_ENTRY(tmp, AT_PHDR, auxargs->phdr);
  AUXARGS_ENTRY(tmp, AT_PHENT, auxargs->phent);
  AUXARGS_ENTRY(tmp, AT_PHNUM, auxargs->phnum);
  AUXARGS_ENTRY(tmp, AT_PAGESZ, auxargs->pagesz);
  AUXARGS_ENTRY(tmp, AT_FLAGS, auxargs->flags);
  AUXARGS_ENTRY(tmp, AT_ENTRY, auxargs->entry);
  AUXARGS_ENTRY(tmp, AT_BASE, auxargs->base);
  AUXARGS_ENTRY(tmp, AT_NULL, 0);

  #ifdef DEBUG
  kprintf("AT_BASE: [0x%X]\n",auxargs->base);
  #endif

  return;
  }

/***
 END
 ***/
