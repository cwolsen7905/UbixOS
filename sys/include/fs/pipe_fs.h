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

#ifndef _FS_PIPE_FS_H
#define _FS_PIPE_FS_H

struct pipe_inode_info {
    struct wait_queue *wait;
    char *base;
    unsigned int start;
    unsigned int len;
    unsigned int lock;
    unsigned int rd_openers;
    unsigned int wr_openers;
    unsigned int readers;
    unsigned int writers;
};

#define PIPE_WAIT(inode)  ((inode).u.pipe_i.wait)
#define PIPE_BASE(inode)  ((inode).u.pipe_i.base)
#define PIPE_START(inode) ((inode).u.pipe_i.start)
#define PIPE_LEN(inode)   ((inode).u.pipe_i.len)
#define PIPE_RD_OPENERS(inode)  ((inode).u.pipe_i.rd_openers)
#define PIPE_WR_OPENERS(inode)  ((inode).u.pipe_i.wr_openers)
#define PIPE_READERS(inode) ((inode).u.pipe_i.readers)
#define PIPE_WRITERS(inode) ((inode).u.pipe_i.writers)
#define PIPE_LOCK(inode)  ((inode).u.pipe_i.lock)
#define PIPE_SIZE(inode)  PIPE_LEN(inode)

#define PIPE_EMPTY(inode) (PIPE_SIZE(inode)==0)
#define PIPE_FULL(inode)  (PIPE_SIZE(inode)==PIPE_BUF)
#define PIPE_FREE(inode)  (PIPE_BUF - PIPE_LEN(inode))
#define PIPE_END(inode)   ((PIPE_START(inode)+PIPE_LEN(inode))&\
                 (PIPE_BUF-1))
#define PIPE_MAX_RCHUNK(inode)  (PIPE_BUF - PIPE_START(inode))
#define PIPE_MAX_WCHUNK(inode)  (PIPE_BUF - PIPE_END(inode))

#endif
