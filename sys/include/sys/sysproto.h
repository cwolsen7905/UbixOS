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

#ifndef _SYS_SYSPROTO_H
#define _SYS_SYSPROTO_H

#include <sys/thread.h>

typedef int register_t;

#define PAD_(t) (sizeof(register_t) <= sizeof(t) ? 0 : sizeof(register_t) - sizeof(t))

#if BYTE_ORDER == LITTLE_ENDIAN
#define PADL_(t)        0
#define PADR_(t)        PAD_(t)
#else
#define PADL_(t)        PAD_(t)
#define PADR_(t)        0
#endif

//Protos
struct sys_mpiCreateMbox_args {
    char name_l_[PADL_(char *)];
    char *name;
    char name_r_[PADR_(char *)];
};

struct sys_mpiDestroyMbox_args {
    char name_l_[PADL_(char *)];
    char *name;
    char name_r_[PADR_(char *)];
};

struct sys_mpiFetchMessage_args {
    char name_l_[PADL_(char *)];
    char *name;
    char name_r_[PADR_(char *)];
    char msg_l_[PADL_(const void *)];
    const void * msg;
    char msg_r_[PADR_(const void *)];
};

struct sys_mpiPostMessage_args {
    char name_l_[PADL_(char *)];
    char *name;
    char name_r_[PADR_(char *)];
    char type_l_[PADL_(uint32_t)];
    uint32_t type;
    char type_r_[PADR_(uint32_t)];
    char msg_l_[PADL_(const void *)];
    const void *msg;
    char msg_r_[PADR_(const void *)];
};

struct sys_sde_args {
  char cmd_l_[PADL_(uint32_t)];
  uint32_t cmd;
  char cmd_r_[PADR_(uint32_t)];
  char ptr_l_[PADL_(void *)];
  void *ptr;
  char ptr_r_[PADR_(void *)];
};


//Func Defs
int sys_invalid(struct thread *, void *);
int sys_mpiCreateMbox(struct thread *, struct sys_mpiCreateMbox_args *);
int sys_mpiDestroyMbox(struct thread *, struct sys_mpiDestroyMbox_args *);
int sys_mpiFetchMessage(struct thread *, struct sys_mpiFetchMessage_args *);
int sys_mpiPostMessage(struct thread *, struct sys_mpiPostMessage_args *);

#endif
