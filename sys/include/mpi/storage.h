/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _KERNEL_MPI_STORAGE_H
#define _KERNEL_MPI_STORAGE_H

#include <sys/types.h>

/*
 * Kernel-side MPI storage event protocol — matches userland include/mpi/storage.h.
 *
 * USB and IDE drivers post MPI_STORAGE_APPEARED when a device is detected and
 * MPI_STORAGE_DEPARTED when it is removed.  automountd handles both events.
 *
 * Payload must fit in mpi_message_t.data[248].
 * 4 + 64 + 32 + 16 + 96 = 212 bytes — fits.
 */

#define AUTOMOUNTD_MBOX         "automountd"

#define MPI_STORAGE_APPEARED    0x5100u
#define MPI_STORAGE_DEPARTED    0x5101u

#define STORAGE_DEV_MAX         64
#define STORAGE_VOL_MAX         32
#define STORAGE_FS_MAX          16
#define STORAGE_MNT_MAX         96

typedef struct {
	u_int32_t type;                        /* MPI_STORAGE_APPEARED / DEPARTED */
	char     dev_path[STORAGE_DEV_MAX];   /* e.g. "/dev/uba0" */
	char     volume_name[STORAGE_VOL_MAX];/* FAT volume label, e.g. "UBIX" */
	char     fstype[STORAGE_FS_MAX];      /* "fat" */
	char     mountpath[STORAGE_MNT_MAX];  /* filled by automountd on DEPARTED */
} mpi_storage_msg_t;

#endif /* _KERNEL_MPI_STORAGE_H */
