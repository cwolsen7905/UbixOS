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
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 0 - syscall
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 1 -
  { ARG_COUNT(sys_mpiCreateMbox_args), "mpiCrateMbox", sys_mpiCreateMbox, SYSCALL_VALID},  // 50 - mpiCreateMbox
  { ARG_COUNT(sys_mpiDestroyMbox_args), "mpiDestroyMbox", sys_mpiDestroyMbox, SYSCALL_VALID },  // 51 - mpiDestroyMbox
  { ARG_COUNT(sys_mpiPostMessage_args), "mpiPostMessage", sys_mpiPostMessage, SYSCALL_VALID },  // 52 - mpiPostMessage
  { ARG_COUNT(sys_mpiFetchMessage_args), "mpiFetchMEssage", sys_mpiFetchMessage, SYSCALL_VALID },  // 53 - mpiFetchMessage
  { 0, "No Call", sys_invalid, SYSCALL_VALID },  // 54 - mpiSpam
  };

int totalCalls = sizeof(systemCalls) / sizeof(struct syscall_entry);
