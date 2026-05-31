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

#include <ubixos/syscalls.h>
#include <sys/sysproto.h>

/* System Calls List */
struct syscall_entry systemCalls[] = {
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 0 - syscall
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 2 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 3 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 4 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 5 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 6 -
    {ARG_COUNT(sys_pidStatus_args), "pidStatus", (sys_call_t *)sys_pidStatus, SYSCALL_VALID}, // 7 - pidStatus
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 1 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 39 -
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 40 - (retired: sysSDE)
    {ARG_COUNT(sys_getvfscwd_args), "getvfscwd", (sys_call_t *)sys_getvfscwd, SYSCALL_VALID}, // 41 -
    {ARG_COUNT(sys_ttyctrl_args), "ttyctrl", (sys_call_t *)sys_ttyctrl, SYSCALL_VALID},       // 42 -
    {ARG_COUNT(sys_mapfb_args), "mapfb", (sys_call_t *)sys_mapfb, SYSCALL_VALID},             // 43 -
    {ARG_COUNT(sys_getmouse_args), "getmouse", (sys_call_t *)sys_getmouse, SYSCALL_VALID},    // 44 -
    {ARG_COUNT(sys_shareregion_args), "shareregion", (sys_call_t *)sys_shareregion, SYSCALL_VALID}, // 45 -
    {ARG_COUNT(sys_getkbd_args), "getkbd", (sys_call_t *)sys_getkbd, SYSCALL_VALID},                // 46 -
    {ARG_COUNT(sys_klog_read_args), "klog_read", (sys_call_t *)sys_klog_read, SYSCALL_VALID},       // 47 -
    {ARG_COUNT(sys_settty_args), "settty", (sys_call_t *)sys_settty, SYSCALL_VALID},                // 48 -
    {ARG_COUNT(sys_klog_write_args), "klog_write", (sys_call_t *)sys_klog_write, SYSCALL_VALID},    // 49 -
    {ARG_COUNT(sys_mpiCreateMbox_args),
     "mpiCrateMbox",
     (sys_call_t *)sys_mpiCreateMbox,
     SYSCALL_VALID}, // 50 - mpiCreateMbox
    {ARG_COUNT(sys_mpiDestroyMbox_args),
     "mpiDestroyMbox",
     (sys_call_t *)sys_mpiDestroyMbox,
     SYSCALL_VALID}, // 51 - mpiDestroyMbox
    {ARG_COUNT(sys_mpiPostMessage_args),
     "mpiPostMessage",
     (sys_call_t *)sys_mpiPostMessage,
     SYSCALL_VALID}, // 52 - mpiPostMessage
    {ARG_COUNT(sys_mpiFetchMessage_args),
     "mpiFetchMEssage",
     (sys_call_t *)sys_mpiFetchMessage,
     SYSCALL_VALID},                                                                          // 53 - mpiFetchMessage
    {0, "No Call", sys_invalid, SYSCALL_VALID},                                               // 54 - mpiSpam
    {ARG_COUNT(sys_ptyalloc_args), "ptyalloc", (sys_call_t *)sys_ptyalloc, SYSCALL_VALID},    // 55 - ptyalloc
    {ARG_COUNT(sys_ptyfree_args), "ptyfree", (sys_call_t *)sys_ptyfree, SYSCALL_VALID},       // 56 - ptyfree
    {ARG_COUNT(sys_ptyinject_args), "ptyinject", (sys_call_t *)sys_ptyinject, SYSCALL_VALID}, // 57 - ptyinject
    {ARG_COUNT(sys_ptysnap_args), "ptysnap", (sys_call_t *)sys_ptysnap, SYSCALL_VALID},       // 58 - ptysnap
    {ARG_COUNT(sys_net_configure_args),
     "net_configure",
     (sys_call_t *)sys_net_configure,
     SYSCALL_VALID}, // 59 - net_configure
    {ARG_COUNT(sys_vesa_modes_args), "vesa_modes", (sys_call_t *)sys_vesa_modes, SYSCALL_VALID}, // 60 - vesa_modes
};

int totalCalls = sizeof(systemCalls) / sizeof(struct syscall_entry);
