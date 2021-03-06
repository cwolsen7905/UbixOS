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

#ifndef _UBIXOS_INIT_H
#define _UBIXOS_INIT_H

#include <vmm/vmm.h>
#include <vfs/vfs.h>
#include <isa/8259.h>
#include <sys/idt.h>
#include <ubixos/sched.h>
#include <isa/pit.h>
#include <isa/atkbd.h>
#include <ubixos/time.h>
#include <net/net.h>
#include <isa/ne2k.h>
#include <devfs/devfs.h>
#include <pci/pci.h>
#include <ubixfs/ubixfs.h>
#include <isa/fdc.h>
#include <ubixos/tty.h>
#include <ufs/ufs.h>
#include <fat/fat.h>
#include <ubixos/static.h>
#include <pci/hd.h>
#include <sys/kern_sysctl.h>
#include <ubixos/vitals.h>
#include <ubixos/syscalls.h>
#include <pci/lnc.h>

typedef int (*intFunctionPTR)(void);

intFunctionPTR init_tasks[] = { static_constructors, i8259_init, idt_init, vitals_init, sysctl_init, vfs_init, sched_init, pit_init, atkbd_init, time_init, pci_init, devfs_init, tty_init, ufs_init, fat_init, initHardDisk, initLNC, net_init };

//ne2k_init,
//ubixfs_init,
//fdc_init,

int init_tasksTotal = sizeof(init_tasks) / sizeof(intFunctionPTR);

#endif /* END _UBIXOS_INIT_H */
