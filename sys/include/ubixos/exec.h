/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
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

#ifndef _UBIXOS_EXEC_H
#define _UBIXOS_EXEC_H

#include <sys/types.h>
#include <ubixos/sched.h>

u_int32_t execThread(void (*tproc)(void), u_int32_t, char *, const char *name);
void execFile(char *file, char **argv, char **envp, int console);

int sys_exec(struct thread *, char *, char **, char **);

/* MI execve helper (sys/kern/kern_exec.c): set a task's name (basename of path)
 * and cmdline (argv joined) on exec.  Shared by the per-arch exec paths. */
void exec_set_name_cmdline(kTask_t *t, const char *path, char **argv, int argc);

/* Upper bound on an execve's argv + envp: the strings, their vector slots and
 * the fixed part of the stack vector.  The loaders reserve this much of the new
 * stack for it.  Equal to musl's ARG_MAX. */
#define EXEC_ARG_MAX (128 * 1024)
/* Fixed part of the stack vector: argc, two NULL terminators, 8 auxv pairs,
 * AT_RANDOM and alignment slack. */
#define EXEC_ARG_FIXED (48 + 19 * 8)
/* Longest executable path execve accepts (PATH_MAX). */
#define EXEC_PATH_MAX 1024
/* String area: the arguments, the path and a "#!" interpreter + argument. */
#define EXEC_ARG_BUF (EXEC_ARG_MAX + 3 * EXEC_PATH_MAX)

/* An execve's path, argv and envp, copied into the kernel (sys/kern/kern_exec.c). */
struct exec_args
{
	char *path;  /* program to load */
	char **argv; /* argc entries, NULL-terminated */
	int argc;
	char **envp; /* envc entries, NULL-terminated */
	int envc;
	char *buf;          /* string area (EXEC_ARG_BUF bytes) */
	unsigned long used; /* bytes of buf in use */
	unsigned long room; /* EXEC_ARG_MAX budget left */
	char **vec;         /* backing for argv and envp */
};

/* Copy path/argv/envp out of the caller: 0, or -EFAULT/-E2BIG/-ENAMETOOLONG/-ENOMEM. */
int exec_args_copyin(struct exec_args *ea, const char *upath, char *const *uargv, char *const *uenvp);
/* Rewrite @ea for a "#!" script: 1 rewritten, 0 not a script, -E2BIG. */
int exec_args_script(struct exec_args *ea);
void exec_args_free(struct exec_args *ea);

#endif /* END _UBIXOS_EXEC_H */
