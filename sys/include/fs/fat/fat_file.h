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

#ifndef _FAT_FILE_H
#define _FAT_FILE_H

#include <fs/fat/fat_internal.h>

/*
 * Open a file.  mode is FAT_MODE_R, FAT_MODE_W, or FAT_MODE_A.
 *   R — file must exist; open for reading.
 *   W — create if needed, truncate if found; open for writing.
 *   A — create if needed, seek to EOF; open for append.
 * Returns a heap-allocated fat_file on success, NULL on failure.
 */
struct fat_file	*fat_file_open(struct fat_fs *fs, const char *path,
		    u_int8_t mode);

/*
 * Read up to size bytes at the current position.
 * *got is set to the number of bytes actually read (may be less at EOF).
 * Returns 0 on success, -1 on I/O error.
 */
int		 fat_file_read(struct fat_file *f, void *buf, u_int32_t size,
		    u_int32_t *got);

/*
 * Write size bytes at the current position, extending the file as needed.
 * Returns 0 on success, -1 on error.
 */
int		 fat_file_write(struct fat_file *f, const void *buf,
		    u_int32_t size);

/*
 * Seek to an absolute byte position within the file.
 * For reads: pos must be <= file_size.
 * For writes: pos must be <= file_size (writing extends at current position).
 * Returns 0 on success, -1 on error.
 */
int		 fat_file_seek(struct fat_file *f, u_int32_t pos);

/*
 * Flush the write-behind data buffer and the directory entry size to disk.
 * Both data and metadata are committed; this is the fix for the fl_fflush bug.
 */
int		 fat_file_flush(struct fat_file *f);

/* Flush and free the fat_file structure. */
void		 fat_file_close(struct fat_file *f);

#endif /* _FAT_FILE_H */
