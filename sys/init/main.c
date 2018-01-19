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

#include <ubixos/init.h>
#include <sys/gdt.h>
#include <sys/video.h>
#include <sys/tss.h>
#include <ubixos/exec.h>
#include <ubixos/kpanic.h>
#include <ubixos/systemtask.h>
#include <vfs/mount.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>

#define B_ADAPTORSHIFT          24
#define B_ADAPTORMASK           0x0f
#define B_ADAPTOR(val)          (((val) >> B_ADAPTORSHIFT) & B_ADAPTORMASK)
#define B_CONTROLLERSHIFT       20
#define B_CONTROLLERMASK        0xf
#define B_CONTROLLER(val)       (((val)>>B_CONTROLLERSHIFT) & B_CONTROLLERMASK)
#define B_SLICESHIFT            20
#define B_SLICEMASK             0xff
#define B_SLICE(val)            (((val)>>B_SLICESHIFT) & B_SLICEMASK)
#define B_UNITSHIFT             16
#define B_UNITMASK              0xf
#define B_UNIT(val)             (((val) >> B_UNITSHIFT) & B_UNITMASK)
#define B_PARTITIONSHIFT        8
#define B_PARTITIONMASK         0xff
#define B_PARTITION(val)        (((val) >> B_PARTITIONSHIFT) & B_PARTITIONMASK)
#define B_TYPESHIFT             0
#define B_TYPEMASK              0xff
#define B_TYPE(val)             (((val) >> B_TYPESHIFT) & B_TYPEMASK)

/*****************************************************************************************
 Desc: The Kernels Descriptor table:
 0 - 0x00 - Dummy Entry
 1 - 0x08 - Ring 0 CS
 2 - 0x10 - Ring 0 DS
 3 - 0x18 - LDT
 4 - 0x20 - Scheduler TSS
 5 - 0x28 - Ring 3 CS
 6 - 0x30 - Ring 3 DS
 7 - 0x38 - GPF TSS
 8 - 0x40 - Stack Fault TSS
 9 - 0x48 - SMP Private Data
 10 - 0x50 - USER %GS (Stack!)

 Notes:

 MrOlsen: test

 *****************************************************************************************/
ubixDescriptorTable(ubixGDT, 11) {
{ .dummy = 0},
ubixStandardDescriptor(0x0000, 0xFFFFF, (dCode + dRead + dBig + dBiglim)),
ubixStandardDescriptor(0x0000, 0xFFFFF, (dData + dWrite + dBig + dBiglim)),
ubixStandardDescriptor(VMM_USER_START, 0xFFFFF, (dLdt)),
ubixStandardDescriptor(0x4200, (sizeof(struct tssStruct)), (dTss + dDpl3)),
ubixStandardDescriptor(0x0000, 0xFFFFF, (dCode + dRead + dBig + dBiglim + dDpl3)),
ubixStandardDescriptor(0x0000, 0xFFFFF, (dData + dWrite + dBig + dBiglim + dDpl3)),
ubixStandardDescriptor(0x4200, (sizeof(struct tssStruct)), (dTss)),
ubixStandardDescriptor(0x6200, (sizeof(struct tssStruct)), (dTss)),
ubixStandardDescriptor(0x0000, 0xFFFFF, (dData + dWrite + dBig + dBiglim + dDpl0)),
ubixStandardDescriptor(0xBFC00000, 0xFFFFF, (dData + dWrite + dBig + dBiglim + dDpl3)),
};

struct {
  unsigned short limit __attribute__ ((packed));
  union descriptorTableUnion *gdt __attribute__ ((packed));
} loadGDT = { (11 * sizeof(union descriptorTableUnion) - 1), ubixGDT };

static char *argv_init[2] = { "init", NULL, }; // ARGV For Initial Proccess
static char *envp_init[6] = { "HOME=/", "PWD=/", "PATH=/bin:/sbin:/usr/bin:/usr/sbin", "USER=root", "GROUP=admin", NULL, }; //ENVP For Initial Proccess

/**
 * \brief This is the entry point into the os where all of the kernels sub systems are started up.
 *
 * \param rootdev address of root device structure
 */
int kmain(uInt32 rootdev) {
  /* Set up counter for startup routine */
  int i = 0x0;
  uInt32 *sysTask = 0x0;

  /* Do A Clear Screen Just To Make The TEXT Buffer Nice And Empty */
  clearScreen();

  /* Modify src/sys/include/ubixos/init.h to add a startup routine */
  for (i = 0x0; i < init_tasksTotal; i++) {
    if (init_tasks[i]() != 0x0)
      kpanic("Error: Initializing System Task[%i].\n", i);
  }

  /* New Root Mount Point */
  //Old 2 new 10
  kprintf("[0x%X][0x%X:0x%X:0x%X:0x%X:0x%X:0x%X]\n", B_ADAPTOR(rootdev), B_CONTROLLER(rootdev), B_SLICE(rootdev), B_UNIT(rootdev), B_PARTITION(rootdev), B_TYPE(rootdev));
  //if ( vfs_mount( 0x1, B_PARTITION(rootdev) + 2, 0x0, 0xAA, "sys", "rw" ) != 0x0 ) {
  if (vfs_mount(0x1, 0x2, 0x0, 0xAA, "sys", "rw") != 0x0) {
    kprintf("Problem Mounting sys Mount Point\n");
  }
  else
    kprintf("Mounted sys\n");

  /* Do our mounting */
  /*
   if (vfs_mount(0x0,0x0,0x0,0x0,"sys","rw") != 0x0) {
   kprintf("Problem Mounting sys Mount Point\n");
   }
   if (vfs_mount(0x0,0x0,0x1,0x0,"tmp","rw") != 0x0) {
   kprintf("Problem Mounting tmp Mount Point\n");
   }
   */

  /* Initialize the system */
  kprintf("Free Pages: [%i]\n", systemVitals->freePages);
  kprintf("MemoryMap:  [0x%X]\n", vmmMemoryMap);
  kprintf("Starting OS\n");

  sysTask = kmalloc(0x2000);

  if (sysTask == 0x0)
    kprintf("OS: Unable to allocate memory\n");

  execThread(systemTask, (uInt32) sysTask + 0x2000, 0x0);
  kprintf("Thread Start!\n");

  execFile("sys:/bin/init", argv_init, envp_init, 0x0); /* OS Initializer    */
  kprintf("File Start!\n");

  irqEnable(0x0);

  while (0x1)
    asm("hlt");

  /* Keep haulting until the scheduler reacts */

  /* Return to start however we should never get this far */
  return (0x0);
}

/***
 END
 ***/
