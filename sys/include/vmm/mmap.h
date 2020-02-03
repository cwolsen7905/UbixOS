/*-
 * Copyright (c) 2020 The UbixOS Project.
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

#ifndef _VMM_MMAP_H
#define _VMM_MMAP_H

// @formatter:off
typedef __uint32_t  __vm_size_t;    // MrOlsen (2016-01-15) TEMP: Put These somewhere else
typedef __vm_size_t vm_size_t;      // vm_size_t
// @formatter:on

#define EINVAL                  22  // Invalid argument
#define MAP_ALIGNED(n)          ((n) << MAP_ALIGNMENT_SHIFT)
#define MAP_ALIGNMENT_SHIFT     24
#define MAP_ALIGNMENT_MASK      MAP_ALIGNED(0xff)
#define MAP_ALIGNED_SUPER       MAP_ALIGNED(1) /* align on a superpage */
#define NBBY                    8               /* number of bits in a byte */

/* PROTs */
#define PROT_NONE       0x00    /* no permissions */
#define PROT_READ       0x01    /* pages can be read */
#define PROT_WRITE      0x02    /* pages can be written */
#define PROT_EXEC       0x04    /* pages can be executed */

/* FLAGs */

/*
 * Do I Need These?
 */
#define MAP_SHARED       0x0001 /* share changes */
#define MAP_PRIVATE      0x0002 /* changes are private */
#define MAP_FIXED        0x0010 /* map addr must be exactly as requested */
#define MAP_RENAME       0x0020 /* Sun: rename private pages to file */
#define MAP_NORESERVE    0x0040 /* Sun: don't reserve needed swap area */
#define MAP_RESERVED0080 0x0080 /* previously misimplemented MAP_INHERIT */
#define MAP_RESERVED0100 0x0100 /* previously unimplemented MAP_NOEXTEND */
#define MAP_HASSEMAPHORE 0x0200 /* region may contain semaphores */
#define MAP_STACK        0x0400 /* region grows down, like a stack */
#define MAP_NOSYNC       0x0800 /* page to but do not sync underlying file */

/*
 * Mapping type
 */
#define MAP_FILE         0x0000 /* map from file (default) */
#define MAP_ANON         0x1000 /* allocated from memory, swap space */

#endif
