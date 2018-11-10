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

//#include <ubixos/sched.h>
#include <sys/thread.h>
#include <sys/sysproto_posix.h>
#include <sys/pipe.h>
#include <sys/descrip.h>
#include <sys/video.h>
#include <string.h>
//#include <ufs/ufs.h>

int sys_pipe2(struct thread *thr, struct sys_pipe2_args *args) {
  int error = 0x0;

  int fd1 = 0x0;
  int fd2 = 0x0;

  struct file *nfp1 = 0x0;
  struct file *nfp2 = 0x0;

  struct pipeInfo *pipeDesc = kmalloc(sizeof(struct pipeInfo));
  memset(pipeDesc, 0x0, sizeof(struct pipeInfo));

  error = falloc(thr, &nfp1, &fd1);
  error = falloc(thr, &nfp2, &fd2);

  nfp1->data = pipeDesc;
  nfp2->data = pipeDesc;

  nfp1->fd_type = 3;
  nfp2->fd_type = 3;

  pipeDesc->rFD = fd1;
  pipeDesc->rfdCNT = 2;
  pipeDesc->wFD = fd2;
  pipeDesc->wfdCNT = 2;

  args->fildes[0] = fd1;
  args->fildes[1] = fd2;

  thr->td_retval[0] = 0;

  return (0x0);
}
