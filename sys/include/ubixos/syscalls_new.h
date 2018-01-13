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

#ifndef _SYSCALLS_NEW_H
#define _SYSCALLS_NEW_H

#include <sys/sysproto.h>

int sysExit();
int read();
int getpid();
int fcntl();
int issetugid();
int __sysctl();
int pipe();
int readlink();
int getuid();
int getgid();
int close();
int mmap();
int obreak();
int sigaction();
int getdtablesize();
int munmap();
int sigprocmask();
int gettimeofday_new();
int fstat();
int ioctl();

#define  invalid_call    0x0
#define  PSL_C           0x00000001      /* carry bit */
#define  EJUSTRETURN     (-2)            /* don't modify regs, just return */
#define  ERESTART        (-1)            /* restart syscall */

typedef int (*functionPTR)();

/*!
 * \brief Mast System Call List
 */
functionPTR systemCalls_new[] = {
invalid_call, /**   0 **/
sysExit, /**   1 **/
invalid_call, /**   2 **/
read, /**   3 **/
sys_write, /**   4 **/
sys_open, /**   5 **/
close, /**   6 **/
invalid_call, /**   7 **/
invalid_call, /** 8 **/
invalid_call, /** 9 **/
invalid_call, /** 10 **/
invalid_call, /** 11 **/
invalid_call, /** 12 **/
invalid_call, /** 13 **/
invalid_call, /** 14 **/
invalid_call, /** 15 **/
invalid_call, /** 16 **/
obreak, /** 17 **/
invalid_call, /** 18 **/
invalid_call, /** 19 **/
getpid, /** 20 **/
invalid_call, /** 21 **/
invalid_call, /** 22 **/
invalid_call, /** 23 **/
getuid, /** 24 **/
invalid_call, /** 25 **/
invalid_call, /** 26 **/
invalid_call, /** 27 **/
invalid_call, /** 28 **/
invalid_call, /** 29 **/
invalid_call, /** 30 **/
invalid_call, /** 31 **/
invalid_call, /** 32 **/
access, /** 33 **/
invalid_call, /** 34 **/
invalid_call, /** 35 **/
invalid_call, /** 36 **/
invalid_call, /** 37 **/
invalid_call, /** 38 **/
invalid_call, /** 39 **/
invalid_call, /** 40 **/
invalid_call, /** 41 **/
pipe, /** 42 **/
invalid_call, /** 43 **/
invalid_call, /** 44 **/
invalid_call, /** 45 **/
invalid_call, /** 46 **/
getgid, /** 47 **/
invalid_call, /** 48 **/
invalid_call, /** 49 **/
invalid_call, /** 50 **/
invalid_call, /** 51 **/
invalid_call, /** 52 **/
invalid_call, /** 53 **/
ioctl, /** 54 **/
invalid_call, /** 55 **/
invalid_call, /** 56 **/
invalid_call, /** 57 **/
readlink, /** 58 **/
invalid_call, /** 59 **/
invalid_call, /** 60 **/
invalid_call, /** 61 **/
invalid_call, /** 62 **/
invalid_call, /** 63 **/
invalid_call, /** 64 **/
invalid_call, /** 65 **/
invalid_call, /** 66 **/
invalid_call, /** 67 **/
invalid_call, /** 68 **/
invalid_call, /** 69 **/
invalid_call, /** 70 **/
invalid_call, /** 71 **/
invalid_call, /** 72 **/
munmap, /** 73 **/
mprotect, /** 74 **/
invalid_call, /** 75 **/
invalid_call, /** 76 **/
invalid_call, /** 77 **/
invalid_call, /** 78 **/
invalid_call, /** 79 **/
invalid_call, /** 80 **/
invalid_call, /** 81 **/
invalid_call, /** 82 **/
setitimer, /** 83 **/
invalid_call, /** 84 **/
invalid_call, /** 85 **/
invalid_call, /** 86 **/
invalid_call, /** 87 **/
invalid_call, /** 88 **/
getdtablesize, /** 89 **/
invalid_call, /** 90 **/
invalid_call, /** 91 **/
fcntl, /** 92 **/
invalid_call, /** 93 **/
invalid_call, /** 94 **/
invalid_call, /** 95 **/
invalid_call, /** 96 **/
invalid_call, /** 97 **/
invalid_call, /** 98 **/
invalid_call, /** 99 **/
invalid_call, /** 100 **/
invalid_call, /** 101 **/
invalid_call, /** 102 **/
invalid_call, /** 103 **/
invalid_call, /** 104 **/
invalid_call, /** 105 **/
invalid_call, /** 106 **/
invalid_call, /** 107 **/
invalid_call, /** 108 **/
invalid_call, /** 109 **/
invalid_call, /** 110 **/
invalid_call, /** 111 **/
invalid_call, /** 112 **/
invalid_call, /** 113 **/
invalid_call, /** 114 **/
invalid_call, /** 115 **/
gettimeofday_new, /** 116 **/
invalid_call, /** 117 **/
invalid_call, /** 118 **/
invalid_call, /** 119 **/
invalid_call, /** 120 **/
invalid_call, /** 121 **/
invalid_call, /** 122 **/
invalid_call, /** 123 **/
invalid_call, /** 124 **/
invalid_call, /** 125 **/
invalid_call, /** 126 **/
invalid_call, /** 127 **/
invalid_call, /** 128 **/
invalid_call, /** 129 **/
invalid_call, /** 130 **/
invalid_call, /** 131 **/
invalid_call, /** 132 **/
invalid_call, /** 133 **/
invalid_call, /** 134 **/
invalid_call, /** 135 **/
invalid_call, /** 136 **/
invalid_call, /** 137 **/
invalid_call, /** 138 **/
invalid_call, /** 139 **/
invalid_call, /** 140 **/
invalid_call, /** 141 **/
invalid_call, /** 142 **/
invalid_call, /** 143 **/
invalid_call, /** 144 **/
invalid_call, /** 145 **/
invalid_call, /** 146 **/
invalid_call, /** 147 **/
invalid_call, /** 148 **/
invalid_call, /** 149 **/
invalid_call, /** 150 **/
invalid_call, /** 151 **/
invalid_call, /** 152 **/
invalid_call, /** 153 **/
invalid_call, /** 154 **/
invalid_call, /** 155 **/
invalid_call, /** 156 **/
invalid_call, /** 157 **/
invalid_call, /** 158 **/
invalid_call, /** 159 **/
invalid_call, /** 160 **/
invalid_call, /** 161 **/
invalid_call, /** 162 **/
invalid_call, /** 163 **/
invalid_call, /** 164 **/
invalid_call, /** 165 **/
invalid_call, /** 166 **/
invalid_call, /** 167 **/
invalid_call, /** 168 **/
invalid_call, /** 169 **/
invalid_call, /** 170 **/
invalid_call, /** 171 **/
invalid_call, /** 172 **/
invalid_call, /** 173 **/
invalid_call, /** 174 **/
invalid_call, /** 175 **/
invalid_call, /** 176 **/
invalid_call, /** 177 **/
invalid_call, /** 178 **/
invalid_call, /** 179 **/
invalid_call, /** 180 **/
invalid_call, /** 181 **/
invalid_call, /** 182 **/
invalid_call, /** 183 **/
invalid_call, /** 184 **/
invalid_call, /** 185 **/
invalid_call, /** 186 **/
invalid_call, /** 187 **/
invalid_call, /** 188 **/
fstat, /** 189 **/
invalid_call, /** 190 **/
invalid_call, /** 191 **/
invalid_call, /** 192 **/
invalid_call, /** 193 **/
invalid_call, /** 194 **/
invalid_call, /** 195 **/
invalid_call, /** 196 **/
mmap, /** 197 **/
invalid_call, /** 198 **/
invalid_call, /** 199 **/
invalid_call, /** 200 **/
invalid_call, /** 201 **/
__sysctl, /** 202 **/
invalid_call, /** 203 **/
invalid_call, /** 204 **/
invalid_call, /** 205 **/
invalid_call, /** 206 **/
invalid_call, /** 207 **/
invalid_call, /** 208 **/
invalid_call, /** 209 **/
invalid_call, /** 210 **/
invalid_call, /** 211 **/
invalid_call, /** 212 **/
invalid_call, /** 213 **/
invalid_call, /** 214 **/
invalid_call, /** 215 **/
invalid_call, /** 216 **/
invalid_call, /** 217 **/
invalid_call, /** 218 **/
invalid_call, /** 219 **/
invalid_call, /** 220 **/
invalid_call, /** 221 **/
invalid_call, /** 222 **/
invalid_call, /** 223 **/
invalid_call, /** 224 **/
invalid_call, /** 225 **/
invalid_call, /** 226 **/
invalid_call, /** 227 **/
invalid_call, /** 228 **/
invalid_call, /** 229 **/
invalid_call, /** 230 **/
invalid_call, /** 231 **/
invalid_call, /** 232 **/
invalid_call, /** 233 **/
invalid_call, /** 234 **/
invalid_call, /** 235 **/
invalid_call, /** 236 **/
invalid_call, /** 237 **/
invalid_call, /** 238 **/
invalid_call, /** 239 **/
invalid_call, /** 240 **/
invalid_call, /** 241 **/
invalid_call, /** 242 **/
invalid_call, /** 243 **/
invalid_call, /** 244 **/
invalid_call, /** 245 **/
invalid_call, /** 246 **/
invalid_call, /** 247 **/
invalid_call, /** 248 **/
invalid_call, /** 249 **/
invalid_call, /** 250 **/
invalid_call, /** 251 **/
invalid_call, /** 252 **/
issetugid, /** 253 **/
invalid_call, /** 254 **/
invalid_call, /** 255 **/
invalid_call, /** 256 **/
invalid_call, /** 257 **/
invalid_call, /** 258 **/
invalid_call, /** 259 **/
invalid_call, /** 260 **/
invalid_call, /** 261 **/
invalid_call, /** 262 **/
invalid_call, /** 263 **/
invalid_call, /** 264 **/
invalid_call, /** 265 **/
invalid_call, /** 266 **/
invalid_call, /** 267 **/
invalid_call, /** 268 **/
invalid_call, /** 269 **/
invalid_call, /** 270 **/
invalid_call, /** 271 **/
invalid_call, /** 272 **/
invalid_call, /** 273 **/
invalid_call, /** 274 **/
invalid_call, /** 275 **/
invalid_call, /** 276 **/
invalid_call, /** 277 **/
invalid_call, /** 278 **/
invalid_call, /** 279 **/
invalid_call, /** 280 **/
invalid_call, /** 281 **/
invalid_call, /** 282 **/
invalid_call, /** 283 **/
invalid_call, /** 284 **/
invalid_call, /** 285 **/
invalid_call, /** 286 **/
invalid_call, /** 287 **/
invalid_call, /** 288 **/
invalid_call, /** 289 **/
invalid_call, /** 290 **/
invalid_call, /** 291 **/
invalid_call, /** 292 **/
invalid_call, /** 293 **/
invalid_call, /** 294 **/
invalid_call, /** 295 **/
invalid_call, /** 296 **/
invalid_call, /** 297 **/
invalid_call, /** 298 **/
invalid_call, /** 299 **/
invalid_call, /** 300 **/
invalid_call, /** 301 **/
invalid_call, /** 302 **/
invalid_call, /** 303 **/
invalid_call, /** 304 **/
invalid_call, /** 305 **/
invalid_call, /** 306 **/
invalid_call, /** 307 **/
invalid_call, /** 308 **/
invalid_call, /** 309 **/
invalid_call, /** 310 **/
invalid_call, /** 311 **/
invalid_call, /** 312 **/
invalid_call, /** 313 **/
invalid_call, /** 314 **/
invalid_call, /** 315 **/
invalid_call, /** 316 **/
invalid_call, /** 317 **/
invalid_call, /** 318 **/
invalid_call, /** 319 **/
invalid_call, /** 320 **/
invalid_call, /** 321 **/
invalid_call, /** 322 **/
invalid_call, /** 323 **/
invalid_call, /** 324 **/
invalid_call, /** 325 **/
invalid_call, /** 326 **/
invalid_call, /** 327 **/
invalid_call, /** 328 **/
invalid_call, /** 329 **/
invalid_call, /** 330 **/
invalid_call, /** 331 **/
invalid_call, /** 332 **/
invalid_call, /** 333 **/
invalid_call, /** 334 **/
invalid_call, /** 335 **/
invalid_call, /** 336 **/
invalid_call, /** 337 **/
invalid_call, /** 338 **/
invalid_call, /** 339 **/
sigprocmask, /** 340 **/
invalid_call, /** 341 **/
invalid_call, /** 342 **/
invalid_call, /** 343 **/
invalid_call, /** 344 **/
invalid_call, /** 345 **/
invalid_call, /** 346 **/
invalid_call, /** 347 **/
invalid_call, /** 348 **/
invalid_call, /** 349 **/
invalid_call, /** 350 **/
invalid_call, /** 351 **/
invalid_call, /** 352 **/
invalid_call, /** 353 **/
invalid_call, /** 354 **/
invalid_call, /** 355 **/
invalid_call, /** 356 **/
invalid_call, /** 357 **/
invalid_call, /** 358 **/
invalid_call, /** 359 **/
invalid_call, /** 360 **/
invalid_call, /** 361 **/
invalid_call, /** 362 **/
invalid_call, /** 363 **/
invalid_call, /** 364 **/
invalid_call, /** 365 **/
invalid_call, /** 366 **/
invalid_call, /** 367 **/
invalid_call, /** 368 **/
invalid_call, /** 369 **/
invalid_call, /** 370 **/
invalid_call, /** 371 **/
invalid_call, /** 372 **/
invalid_call, /** 373 **/
invalid_call, /** 374 **/
invalid_call, /** 375 **/
invalid_call, /** 376 **/
invalid_call, /** 377 **/
invalid_call, /** 378 **/
invalid_call, /** 379 **/
invalid_call, /** 380 **/
invalid_call, /** 381 **/
invalid_call, /** 382 **/
invalid_call, /** 383 **/
invalid_call, /** 384 **/
invalid_call, /** 385 **/
invalid_call, /** 386 **/
invalid_call, /** 387 **/
invalid_call, /** 388 **/
invalid_call, /** 389 **/
invalid_call, /** 390 **/
invalid_call, /** 391 **/
invalid_call, /** 392 **/
invalid_call, /** 393 **/
invalid_call, /** 394 **/
invalid_call, /** 395 **/
invalid_call, /** 396 **/
fstatfs, /** 397 **/
invalid_call, /** 398 **/
invalid_call, /** 399 **/
invalid_call, /** 400 **/
invalid_call, /** 401 **/
invalid_call, /** 402 **/
invalid_call, /** 403 **/
invalid_call, /** 404 **/
invalid_call, /** 405 **/
invalid_call, /** 406 **/
invalid_call, /** 407 **/
invalid_call, /** 408 **/
invalid_call, /** 409 **/
invalid_call, /** 410 **/
invalid_call, /** 411 **/
invalid_call, /** 412 **/
invalid_call, /** 413 **/
invalid_call, /** 414 **/
invalid_call, /** 415 **/
sigaction, /** 416 **/
invalid_call, /** 417 **/
invalid_call, /** 418 **/
invalid_call, /** 419 **/
invalid_call, /** 420 **/
invalid_call, /** 421 **/
invalid_call, /** 422 **/
invalid_call, /** 423 **/
invalid_call, /** 424 **/
invalid_call, /** 425 **/
invalid_call, /** 426 **/
invalid_call, /** 427 **/
invalid_call, /** 428 **/
invalid_call, /** 429 **/
invalid_call, /** 430 **/
invalid_call, /** 431 **/
invalid_call, /** 432 **/
invalid_call, /** 433 **/
invalid_call, /** 434 **/
invalid_call, /** 435 **/
invalid_call, /** 436 **/
invalid_call, /** 437 **/
invalid_call, /** 438 **/
invalid_call, /** 439 **/
invalid_call, /** 440 **/
invalid_call, /** 441 **/
invalid_call, /** 442 **/
invalid_call, /** 443 **/
invalid_call, /** 444 **/
invalid_call, /** 445 **/
invalid_call, /** 446 **/
invalid_call, /** 447 **/
invalid_call, /** 448 **/
invalid_call, /** 449 **/
invalid_call, /** 450 **/
invalid_call, /** 451 **/
invalid_call, /** 452 **/
invalid_call, /** 453 **/
invalid_call, /** 454 **/
invalid_call, /** 455 **/
};

int totalCalls_new = sizeof(systemCalls_new) / sizeof(functionPTR);

#endif

/***
 END
 ***/

