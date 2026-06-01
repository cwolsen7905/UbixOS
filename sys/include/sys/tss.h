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

#ifndef _SYS_TSS_H
#define _SYS_TSS_H

#include <sys/types.h>

struct tssStruct {
    short back_link;
    short back_link_reserved;
    long esp0;
    short ss0;
    short ss0_reserved;
    long esp1;
    short ss1;
    short ss1_reserved;
    long esp2;
    short ss2;
    short ss2_reserved;
    long cr3;
    long eip;
    long eflags;
    long eax, ecx, edx, ebx;
    long esp;
    long ebp;
    long esi;
    long edi;
    short es;
    short es_reserved;
    short cs;
    short cs_reserved;
    short ss;
    short ss_reserved;
    short ds;
    short ds_reserved;
    short fs;
    short fs_reserved;
    short gs;
    short gs_reserved;
    short ldt;
    short ldt_reserved;
    short trace_bitmap;
    short io_map;
    char io_space[8192];
};

struct i387Struct {
    long cwd;
    long swd;
    long twd;
    long fip;
    long fcs;
    long foo;
    long fos;
    long st_space[20]; /* 8*10 bytes for each FP-reg = 80 bytes */
};

struct i386_frame {
    u_int32_t gs;
    u_int32_t fs;
    u_int32_t es;
    u_int32_t ds;
    u_int32_t ss;
    u_int32_t edi;
    u_int32_t esi;
    u_int32_t ebp;
    u_int32_t esp;
    u_int32_t ebx;
    u_int32_t edx;
    u_int32_t ecx;
    u_int32_t eax;
    /*
     u_int32_t vector;
     u_int32_t error_code;
     */
    u_int32_t eip;
    u_int32_t cs;
    u_int32_t flags;
    u_int32_t user_esp;
    u_int32_t user_ss;
};

#endif /* END _SYS_TSS_H */
