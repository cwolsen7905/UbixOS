/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Machine-independent execve helpers shared by the per-arch exec paths
 * (i386 sys_exec in arch/i386/kern/i386_exec.c, aarch64_exec_replace in
 * arch/aarch64/bringup/execfile.c).  The ELF loading itself stays arch-specific
 * by necessity — i386 maps ELF32 segments through the i386 vmm, aarch64 loads
 * ELF64 PIE images via pmap; merging those would be worse architecture, not
 * better.  What the two paths legitimately share is the neutral bookkeeping
 * below.  Part of the Phase 5 convergence work
 * (docs/design/console-and-arch-convergence-plan.md).
 */

#include <ubixos/sched.h>
#include <ubixos/exec.h>
#include <string.h>

/**
 * Set a task's display name and command line from the executable @path and its
 * (kernel-resident) @argv on execve.  @t->name becomes the basename of @path
 * (the component after the last '/'); @t->cmdline becomes argv[0..argc-1] joined
 * with single spaces, both NUL-terminated and truncated to their field sizes.
 *
 * Used by both arch exec paths so process listings (ps / procfs / the activity
 * monitor) show a clean program name and full invocation regardless of arch.
 *
 * @param argv  kernel copies of the argument strings (argv[0..argc-1]); the
 *              user-space vector has already been marshalled into the kernel.
 */
void exec_set_name_cmdline(kTask_t *t, const char *path, char **argv, int argc)
{
	const char *base = path;
	const char *p;
	size_t pos = 0;
	int j;

	for (p = path; *p != '\0'; p++)
		if (*p == '/')
			base = p + 1;
	strncpy(t->name, base, sizeof(t->name) - 1);
	t->name[sizeof(t->name) - 1] = '\0';

	for (j = 0; j < argc && argv != NULL && argv[j] != NULL && pos < sizeof(t->cmdline) - 1; j++)
	{
		size_t alen;

		if (j > 0)
			t->cmdline[pos++] = ' ';
		alen = strlen(argv[j]);
		if (pos + alen >= sizeof(t->cmdline))
			alen = sizeof(t->cmdline) - pos - 1;
		memcpy(t->cmdline + pos, argv[j], alen);
		pos += alen;
	}
	t->cmdline[pos] = '\0';
}
