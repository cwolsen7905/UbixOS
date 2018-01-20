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

#ifndef _SYSCALLS_H
#define _SYSCALLS_H

#include <ubixos/sched.h>
#include <vfs/file.h>
#include <ubixos/syscall.h>

#define SYSCALLS_MAX 1024

#define  PSL_C           0x00000001      /* carry bit */
#define  EJUSTRETURN     (-2)            /* don't modify regs, just return */
#define  ERESTART        (-1)            /* restart syscall */

#define ARG_COUNT(name) (sizeof(struct name) / sizeof(register_t))

#define SYSCALL_INVALID 0
#define SYSCALL_VALID   1
#define SYSCALL_DUMMY   2

typedef int sys_call_t(struct thread *, void *);

struct syscall_entry {
    int sc_args;
    char *sc_name;
    sys_call_t *sc_entry;
    int sc_status;
};

/*
 *
 * Old Style Calls Need Updates
 *
 */
int sysAuth();
int sysPasswd();
int sysAddModule();
int sysRmModule();
int sysGetpid();
int sysExit();
int sysExec();

int sysCheckPid();
int sysGetFreePage();

int sysFgetc();
int sysFopen();
int sysFclose();
int sysFseek();
int sysMkDir();
int sysRmDir();

int sysSDE();
int sysGetDrives();
int sysGetCwd();
int sysChDir();
int sysGetUptime();
int sysGetTime();
int sysStartSDE();
int sysUnlink();
int sysMpiCreateMbox();
int sysMpiDestroyMbox();
int sysMpiPostMessage();
int sysMpiFetchMessage();
int sysMpiSpam();

typedef int (*functionPTR)();

extern int totalCalls_old;
extern functionPTR systemCalls_Old[];

extern int totalCalls;
extern struct syscall_entry systemCalls[];

#endif
