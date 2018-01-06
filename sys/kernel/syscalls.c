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

 $Id: elf.c 141 2016-01-17 02:05:18Z reddawg $

 *****************************************************************************************/

#include <ubixos/syscalls.h>
#include <sys/sysproto.h>

/* System Calls List */
struct syscall_entry systemCalls[] = {
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 0 - Invalid */
    { ARG_COUNT( sys_exit_args ), "exit", (sys_call_t *)sys_exit ,2},
    { ARG_COUNT( sys_fork_args ), "fork", (sys_call_t *)sys_fork , SYSCALL_VALID},
    { ARG_COUNT( sys_read_args ), "read", (sys_call_t *)sys_read ,2 },
    { ARG_COUNT( sys_write_args ), "write", (sys_call_t *)sys_write ,SYSCALL_VALID},
    { ARG_COUNT( sys_open_args ), "open", (sys_call_t *)sys_open ,2 },
    { ARG_COUNT( sys_close_args ), "close", (sys_call_t *)sys_close ,2 },
    { ARG_COUNT( sys_wait4_args ), "wiat4", (sys_call_t *)sys_wait4 , 1},
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*   8 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*   9 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  10 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  11 - Invalid */
    { ARG_COUNT(sys_chdir_args), "Change Dir", (sys_call_t *)sys_chdir, SYSCALL_VALID },         /*  12 - CH Dir */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  13 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  14 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  15 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  16 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  17 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  18 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  19 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  20 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  21 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  22 - Invalid */
    { ARG_COUNT( sys_setUID_args), "Set UID", (sys_call_t *)sys_setUID, SYSCALL_VALID },         /*  23 - Set UID */
    { 0, "Get UID", sys_getUID, SYSCALL_VALID },                                   /*  24 - sys_getUID */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  25 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  26 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  27 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  28 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  29 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  30 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  31 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  32 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  33 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  34 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  35 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  36 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  37 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  38 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  39 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  40 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  41 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  42 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  43 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  44 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  45 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  46 - Invalid */
    { 0, "getuid", sys_getGID, SYSCALL_VALID },                                    /*  47 - sys_getGID  */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  48 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  49 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  50 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  51 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  52 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  53 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  54 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  55 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  56 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  57 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  58 - Invalid */
    { ARG_COUNT( sys_execve_args), "Exec VE", (sys_call_t *)sys_execve, SYSCALL_DUMMY },                                  /*  59 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  60 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  61 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  62 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  63 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  64 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  65 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  66 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  67 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  68 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  69 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  70 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  71 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  72 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  73 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  74 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  75 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  76 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  77 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  78 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  79 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  80 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  81 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  82 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  83 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  84 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  85 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  86 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  87 - Invalid */
    { 0, "Get Free Page", (sys_call_t *)sysGetFreePage, SYSCALL_VALID },                         /*  88 - getFreePage TEMP */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  89 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  90 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  91 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  92 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  93 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  94 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  95 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  96 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  97 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  98 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /*  99 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 100 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 101 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 102 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 103 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 104 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 105 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 106 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 107 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 108 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 109 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 110 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 111 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 112 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 113 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 114 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 115 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 116 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 117 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 118 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 119 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 120 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 121 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 122 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 123 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 124 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 125 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 126 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 127 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 128 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 129 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 130 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 131 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 132 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 133 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 134 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 135 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 136 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 137 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 138 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 139 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 140 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 141 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 142 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 143 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 144 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 145 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 146 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 147 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 148 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 149 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 150 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 151 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 152 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 153 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 154 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 155 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 156 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 157 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 158 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 159 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 160 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 161 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 162 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 163 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 164 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 165 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 166 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 167 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 168 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 169 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 170 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 171 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 172 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 173 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 174 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 175 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 176 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 177 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 178 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 179 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 180 - Invalid */
    { ARG_COUNT(sys_setGID_args), "Set GID", (sys_call_t *)sys_setGID, SYSCALL_VALID },          /* 181 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 182 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 183 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 184 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 185 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 186 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 187 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 188 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 189 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 190 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 191 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 192 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 193 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 194 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 195 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 196 - Invalid */
    { ARG_COUNT(sys_mmap_args), "MMAP", (sys_call_t *)sys_mmap, SYSCALL_VALID },                                  /* 197 - sys_mmap */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 198 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 199 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 200 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 201 - Invalid */
    { ARG_COUNT(sys_sysctl_args), "SYS CTL", (sys_call_t *)sys_sysctl, SYSCALL_VALID },          /* 202 - sys_sysctl */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 203 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 204 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 205 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 206 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 207 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 208 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 209 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 210 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 211 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 212 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 213 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 214 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 215 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 216 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 217 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 218 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 219 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 220 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 221 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 222 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 223 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 224 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 225 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 226 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 227 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 228 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 229 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 230 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 231 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 232 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 233 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 234 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 235 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 236 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 237 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 238 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 239 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 240 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 241 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 242 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 243 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 244 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 245 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 246 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 247 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 248 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 249 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 250 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 251 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 252 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 253 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 254 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 255 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 256 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 257 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 258 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 259 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 260 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 261 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 262 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 263 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 264 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 265 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 266 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 267 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 268 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 269 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 270 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 271 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 272 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 273 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 274 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 275 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 276 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 277 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 278 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 279 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 280 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 281 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 282 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 283 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 284 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 285 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 286 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 287 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 288 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 289 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 290 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 291 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 292 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 293 - Invalid */
    { ARG_COUNT(sys_fseek_args), "FILE Seek", (sys_call_t *)sys_fseek, SYSCALL_VALID },       /* 294 - sys_fseek */
    { ARG_COUNT(sys_fgetc_args), "FILE Get Char", (sys_call_t *)sys_fgetc, SYSCALL_VALID },       /* 295 - sys_fread */
    { ARG_COUNT(sys_fclose_args), "FILE Close", (sys_call_t *)sys_fclose, SYSCALL_VALID },       /* 296 - sys_fread */
    { ARG_COUNT(sys_fread_args), "FILE Read", (sys_call_t *)sys_fread, SYSCALL_VALID },                                  /* 297 - sys_fread */
    { ARG_COUNT(sys_fopen_args), "FILE Open", (sys_call_t *)sys_fopen, SYSCALL_VALID },                                  /* 298 - sys_fopen */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 299 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 300 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 301 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 302 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 303 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 304 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 305 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 306 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 307 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 308 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 309 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 310 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 311 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 312 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 313 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 314 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 315 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 316 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 317 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 318 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 319 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 320 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 321 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 322 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 323 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 324 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 325 - Invalid */
    { ARG_COUNT(sys_getcwd_args), "Get CWD", (sys_call_t *)sys_getcwd, SYSCALL_VALID },                                  /* 326 - sys_getcwd */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 327 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 328 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 329 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 330 - Invalid */
    { 0, "Sched Yield", sys_sched_yield, SYSCALL_VALID },                          /* 331 - sys_sched_yield */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 332 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 333 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 334 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 335 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 336 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 337 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 338 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 339 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 340 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 341 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 342 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 343 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 344 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 345 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 346 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 347 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 348 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 349 - Invalid */
    { 0, "No Call", sys_invalid, SYSCALL_VALID },                                  /* 350 - Invalid */
};

int totalCalls = sizeof(systemCalls) / sizeof(struct syscall_entry);

/***
 END
 ***/

