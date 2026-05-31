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

#include <sys/pipe.h>
#include <sys/sysproto_posix.h>
#include <sys/thread.h>
#include <lib/kprintf.h>
#include <sys/descrip.h>
#include <lib/kmalloc.h>
#include <string.h>

int pipe(struct thread *td, struct pipe_args *uap)
{
	int fd1 = 0, fd2 = 0;
	struct file *nfp1 = 0, *nfp2 = 0;
	struct pipeInfo *pipeDesc = kmalloc(sizeof(struct pipeInfo));

	if (pipeDesc == 0)
	{
		td->td_retval[0] = -1;
		return (-1);
	}

	memset(pipeDesc, 0, sizeof(struct pipeInfo));

	if (falloc(td, &nfp1, &fd1) != 0 || nfp1 == NULL) {
		kfree(pipeDesc);
		td->td_retval[0] = -1;
		return (-1);
	}
	if (falloc(td, &nfp2, &fd2) != 0 || nfp2 == NULL) {
		fdestroy(td, nfp1, fd1);
		kfree(pipeDesc);
		td->td_retval[0] = -1;
		return (-1);
	}

	nfp1->data = pipeDesc;
	nfp2->data = pipeDesc;
	nfp1->fd_type = 3;
	nfp2->fd_type = 3;
	nfp1->pipe_end = PIPE_END_READ;
	nfp2->pipe_end = PIPE_END_WRITE;

	pipeDesc->rFD = fd1;
	pipeDesc->rfdCNT = 1;
	pipeDesc->wFD = fd2;
	pipeDesc->wfdCNT = 1;

	/* Write to the caller's fd array (musl/modern FreeBSD ABI) */
	if (uap->fildes != 0)
	{
		uap->fildes[0] = fd1;
		uap->fildes[1] = fd2;
	}

	/* Also return in registers for old-style callers */
	td->td_retval[0] = 0;
	td->td_retval[1] = 0;

	return (0);
}
