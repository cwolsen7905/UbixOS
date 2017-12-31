/*****************************************************************************************
 Copyright (c) 2002-2004, 2016 The UbixOS Project
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

 $Id: vmm.h 232 2016-01-24 01:47:12Z reddawg $

 *****************************************************************************************/

#ifndef _VMM_VMM_H_
#define _VMM_VMM_H_

#include <sys/types.h>
#include <vmm/paging.h>

#ifdef __cplusplus
extern "C" {
#endif

#define memAvail     1
#define memNotavail  2
#define vmmID       -3

/*
 #define vmmMemoryMapAddr 0xE6667000
 #define VMM_MMAP_ADDR_PMODE2 0xE6667000
 */

#define VMM_MMAP_ADDR_PMODE  VMM_KERN_START /* (PD_BASE_ADDR + PAGE_SIZE) */
#define VMM_MMAP_ADDR_RMODE  0x101000

#define VMM_KERN_END   0xFFFFFFFF
#define VMM_KERN_START 0xC0800000

#define VMM_USER_END   0xBFFFFFFF
#define VMM_USER_START 0x00000000

struct freebsd6_mmap_args {
    char addr_l_[PADL_( caddr_t )];
    caddr_t addr;
    char addr_r_[PADR_( caddr_t )];

    char len_l_[PADL_( size_t )];
    size_t len;
    char len_r_[PADR_( size_t )];

    char prot_l_[PADL_(int)];
    int prot;char prot_r_[PADR_(int)];
    char flags_l_[PADL_(int)];

    int flags;char flags_r_[PADR_(int)];
    char fd_l_[PADL_(int)];
    int fd;char fd_r_[PADR_(int)];

    char pad_l_[PADL_(int)];
    int pad;
    char pad_r_[PADR_(int)];

    char pos_l_[PADL_( off_t )];
    off_t pos;
    char pos_r_[PADR_( off_t )];
};

typedef struct {
    uint32_t pageAddr;
    u_int16_t status;
    u_int16_t reserved;
    pid_t pid;
    int cowCounter;
} mMap;

extern int numPages;
extern mMap *vmmMemoryMap;

int vmm_init();
int vmm_memMapInit();
int countMemory();
uint32_t vmm_findFreePage( pidType pid );
int freePage( uInt32 pageAddr );
int adjustCowCounter( uInt32 baseAddr, int adjustment );
void vmm_freeProcessPages( pidType pid );


#ifdef __cplusplus
}
#endif

#endif // _VMM_VMM_H