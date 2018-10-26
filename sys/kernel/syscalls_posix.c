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
#include <sys/sysproto_posix.h>

/* System Calls List */
struct syscall_entry systemCalls_posix[] = {
  { 0, "No Call", sys_invalid, SYSCALL_VALID },                                    // 0 - syscall
  { ARG_COUNT(sys_exit_args), "exit", (sys_call_t *) sys_exit, SYSCALL_VALID },    // 1 - exit
  { ARG_COUNT(sys_fork_args), "fork", (sys_call_t *) sys_fork, SYSCALL_VALID },    // 2 - fork
  { ARG_COUNT(sys_read_args), "read", (sys_call_t *) sys_read, SYSCALL_VALID },    // 3 - read
  { ARG_COUNT(sys_write_args), "write", (sys_call_t *) sys_write, SYSCALL_VALID }, // 4 - write
  { ARG_COUNT(sys_open_args), "open", (sys_call_t *) sys_open, SYSCALL_VALID },    // 5 - open
  { ARG_COUNT(sys_close_args), "close", (sys_call_t *) sys_close, SYSCALL_VALID }, // 6 - close
  { ARG_COUNT(sys_wait4_args), "wiat4", (sys_call_t *) sys_wait4, SYSCALL_VALID }, // 7 - wait4
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { ARG_COUNT(sys_chdir_args), "Change Dir", (sys_call_t *) sys_chdir, SYSCALL_VALID }, // 12 - chdir
  { ARG_COUNT(sys_fchdir_args), "fchdir", sys_fchdir, SYSCALL_VALID },                  // 13 - fchdir
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { ARG_COUNT(sys_getpid_args), "getpid", sys_getpid, SYSCALL_VALID }, // 20 - getpid
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { ARG_COUNT(sys_setUID_args), "Set UID", (sys_call_t *) sys_setUID, SYSCALL_VALID }, // 23 - setUID
  { 0, "Get UID", sys_getUID, SYSCALL_VALID },
  { ARG_COUNT(sys_geteuid_args), "geteuid", sys_geteuid, SYSCALL_VALID }, // 25 - getuid
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, // 31
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, // 32
  { ARG_COUNT(sys_access_args), "access", sys_access, SYSCALL_VALID }, // 33 - access
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { ARG_COUNT(sys_getppid_args), "getpid", sys_getpid, SYSCALL_VALID }, // 39 - getppid
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { ARG_COUNT(sys_getegid_args), "getegid", sys_getegid, SYSCALL_VALID }, // 43 - getegid
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "getuid", sys_getGID, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { ARG_COUNT(sys_ioctl_args), "ioctl", sys_ioctl, SYSCALL_VALID }, // 54 - ioctl
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID },
  { ARG_COUNT(sys_execve_args), "execve", (sys_call_t *) sys_execve, SYSCALL_VALID },
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  60 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  61 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  62 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  63 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  64 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  65 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  66 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  67 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  68 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  69 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  70 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  71 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  72 - Invalid */
  { ARG_COUNT(sys_munmap_args), "MUNMAP", sys_munmap, SYSCALL_VALID }, /*  73 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  74 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  75 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  76 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  77 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  78 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  79 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  80 - Invalid */
  { ARG_COUNT(sys_getpgrp_args), "getpgrp", sys_getpgrp, SYSCALL_VALID }, //  81 - getpgrp
  { ARG_COUNT(sys_setpgid_args), "setpgid", sys_setpgid, SYSCALL_VALID }, //  82 - setpgid
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  83 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  84 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  85 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  86 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  87 - Invalid */
  { 0, "Get Free Page", (sys_call_t *) sysGetFreePage, SYSCALL_VALID }, /*  88 - getFreePage TEMP */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  89 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  90 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  91 - Invalid */
    { ARG_COUNT(sys_fcntl_args), "fcntl", sys_fcntl, SYSCALL_VALID }, //  92 - fcntl
  { ARG_COUNT(sys_select_args), "select", sys_select, SYSCALL_VALID }, // 93 - select
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  94 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  95 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  96 - Invalid */
  { ARG_COUNT(sys_socket_args), "socket", sys_socket, SYSCALL_VALID }, //  97 - socket
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  98 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /*  99 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 100 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 101 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 102 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 103 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 104 - Invalid */
  { ARG_COUNT(sys_setsockopt_args), "setsockopt", sys_setsockopt, SYSCALL_VALID }, // 105 setsockopt
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 106 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 107 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 108 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 109 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 110 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 111 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 112 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 113 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 114 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 115 - Invalid */
  { ARG_COUNT(sys_gettimeofday_args), "gettimeofday", sys_gettimeofday, SYSCALL_VALID }, // 116 - gettimeofday
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 117 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 118 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 119 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 120 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 121 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 122 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 123 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 124 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 125 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 126 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 127 - Invalid */
  { ARG_COUNT(sys_rename_args), "rename", sys_rename, SYSCALL_VALID }, /* 128 - rename */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 129 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 130 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 131 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 132 - Invalid */
  { ARG_COUNT(sys_sendto_args), "sendto", sys_sendto, SYSCALL_VALID }, // 133 - sendto
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 134 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 135 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 136 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 137 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 138 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 139 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 140 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 141 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 142 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 143 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 144 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 145 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 146 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 147 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 148 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 149 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 150 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 151 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 152 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 153 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 154 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 155 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 156 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 157 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 158 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 159 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 160 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 161 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 162 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 163 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 164 - Invalid */
  { ARG_COUNT(sys_sysarch_args), "sysarch", sys_sysarch, SYSCALL_VALID }, // 165 - sysarch
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 166 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 167 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 168 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 169 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 170 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 171 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 172 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 173 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 174 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 175 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 176 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 177 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 178 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 179 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 180 - Invalid */
  { ARG_COUNT(sys_setGID_args), "Set GID", (sys_call_t *) sys_setGID, SYSCALL_VALID }, // 181 - getgid
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 182 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 183 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 184 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 185 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 186 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 187 - Invalid */
  { ARG_COUNT(sys_stat_args), "FSTAT", (sys_call_t *) sys_stat, SYSCALL_VALID }, /* 188 - sys_stat */
  { ARG_COUNT(sys_fstat_args), "FSTAT", (sys_call_t *) sys_fstat, SYSCALL_VALID }, /* 189 - sys_fstat */
  { ARG_COUNT(sys_lstat_args), "LSTAT", (sys_call_t *) sys_lstat, SYSCALL_VALID }, /* 190 - sys_lstat */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 191 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 192 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 193 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 194 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 195 - Invalid */
  { ARG_COUNT(sys_getdirentries_args), "getdirentries", sys_getdirentries, SYSCALL_VALID }, // 196 - getdirentries
  { ARG_COUNT(sys_mmap_args), "MMAP", (sys_call_t *) sys_mmap, SYSCALL_VALID }, /* 197 - sys_mmap */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 198 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 199 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 200 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 201 - Invalid */
  { ARG_COUNT(sys_sysctl_args), "SYS CTL", (sys_call_t *) sys_sysctl, SYSCALL_VALID }, /* 202 - sys_sysctl */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 203 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 204 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 205 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 206 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 207 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 208 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 209 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 210 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 211 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 212 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 213 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 214 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 215 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 216 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 217 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 218 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 219 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 220 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 221 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 222 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 223 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 224 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 225 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 226 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 227 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 228 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 229 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 230 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 231 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 232 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 233 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 234 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 235 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 236 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 237 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 238 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 239 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 240 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 241 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 242 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 243 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 244 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 245 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 246 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 247 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 248 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 249 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 250 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 251 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 252 - Invalid */
  { ARG_COUNT(sys_issetugid_args), "ISSETUGID", (sys_call_t *) sys_issetugid, SYSCALL_VALID }, /* 253 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 254 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 255 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 256 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 257 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 258 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 259 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 260 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 261 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 262 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 263 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 264 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 265 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 266 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 267 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 268 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 269 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 270 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 271 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 272 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 273 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 274 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 275 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 276 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 277 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 278 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 279 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 280 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 281 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 282 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 283 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 284 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 285 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 286 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 287 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 288 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 289 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 290 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 291 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 292 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 293 - Invalid */
  { ARG_COUNT(sys_fseek_args), "FILE Seek", (sys_call_t *) sys_fseek, SYSCALL_VALID }, /* 294 - sys_fseek */
  { ARG_COUNT(sys_fgetc_args), "FILE Get Char", (sys_call_t *) sys_fgetc, SYSCALL_VALID }, /* 295 - sys_fread */
  { ARG_COUNT(sys_fclose_args), "FILE Close", (sys_call_t *) sys_fclose, SYSCALL_VALID }, /* 296 - sys_fread */
  { ARG_COUNT(sys_fread_args), "FILE Read", (sys_call_t *) sys_fread, SYSCALL_VALID }, /* 297 - sys_fread */
  { ARG_COUNT(sys_fopen_args), "FILE Open", (sys_call_t *) sys_fopen, SYSCALL_VALID }, /* 298 - sys_fopen */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 299 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 300 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 301 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 302 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 303 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 304 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 305 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 306 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 307 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 308 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 309 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 310 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 311 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 312 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 313 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 314 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 315 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 316 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 317 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 318 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 319 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 320 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 321 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 322 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 323 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 324 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 325 - Invalid */
  { ARG_COUNT(sys_getcwd_args), "Get CWD", (sys_call_t *) sys_getcwd, SYSCALL_VALID }, /* 326 - sys_getcwd */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 327 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 328 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 329 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 330 - Invalid */
  { 0, "Sched Yield", sys_sched_yield, SYSCALL_VALID }, /* 331 - sys_sched_yield */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 332 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 333 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 334 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 335 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 336 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 337 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 338 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 339 - Invalid */
  { ARG_COUNT(sys_sigprocmask_args), "sigprocmask", sys_sigprocmask, SYSCALL_VALID }, // 340 - sigprocmask
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 341 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 342 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 343 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 344 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 345 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 346 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 347 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 348 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 349 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 350 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 351 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 352 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 353 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 354 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 355 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 356 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 357 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 358 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 359 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 350 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 351 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 352 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 353 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 354 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 355 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 356 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 357 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 358 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 359 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 360 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 361 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 362 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 363 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 364 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 365 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 366 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 367 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 368 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 369 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 370 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 371 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 372 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 373 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 374 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 375 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 376 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 377 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 378 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 379 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 380 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 381 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 382 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 383 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 384 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 385 - Invalid */
    { ARG_COUNT(sys_statfs_args), "statfs", (sys_call_t *) sys_statfs, SYSCALL_VALID }, // 396 statfs
    { ARG_COUNT(sys_fstatfs_args), "fstatfs", (sys_call_t *) sys_fstatfs, SYSCALL_VALID }, // 397 fstatfs
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 398 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 399 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 400 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 401 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 402 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 403 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 404 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 405 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 306 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 307 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 308 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 409 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 410 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 411 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 412 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 413 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 414 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 415 - Invalid */
  { ARG_COUNT(sys_sigaction_args), "sigaction", sys_sigaction, SYSCALL_VALID }, // 416 - sigaction
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 417 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 418 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 419 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 410 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 421 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 422 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 423 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 424 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 425 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 426 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 427 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 428 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 429 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 430 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 431 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 432 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 433 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 434 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 435 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 436 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 437 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 438 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 439 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 440 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 441 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 442 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 443 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 444 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 445 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 446 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 447 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 448 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 449 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 450 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 451 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 452 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 453 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 454 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 455 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 456 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 457 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 458 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 459 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 460 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 461 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 462 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 463 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 464 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 465 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 466 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 467 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 468 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 469 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 470 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 471 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 472 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 473 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 474 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 475 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 476 - Invalid */
  { ARG_COUNT(sys_mmap_args), "MMAP", (sys_call_t *) sys_mmap, SYSCALL_VALID }, /* 477 - sys_mmap */
  { ARG_COUNT(sys_lseek_args), "lseek", (sys_call_t *) sys_lseek, SYSCALL_VALID }, /* 478 - sys_lseek */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 359 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 350 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 351 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 352 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 353 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 354 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 355 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 356 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 357 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 358 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 359 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 350 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 351 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 352 - Invalid */
  { ARG_COUNT(sys_fstatat_args), "fstatat", sys_fstatat, SYSCALL_VALID }, // 493 - fstatat
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 354 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 355 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 356 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 357 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 358 - Invalid */
  { ARG_COUNT(sys_openat_args), "SYS_openat", sys_openat, SYSCALL_VALID }, /* 499 - sys_openat */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 350 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 351 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 352 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 353 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 354 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 355 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 356 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 357 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 358 - Invalid */
  { 0, "No Call", sys_invalid, SYSCALL_VALID }, /* 359 - Invalid */
};

int totalCalls_posix = sizeof(systemCalls_posix) / sizeof(struct syscall_entry);
