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
#include <fs/vfs/file.h> /* fopen/fread/fclose (the "#!" line) */
#include <lib/kmalloc.h>
#include <sys/errno.h>
#include <ubixos/uaccess.h>

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

/**
 * Append the user string at @ustr to @ea's string area, charging @cost_extra
 * bytes (its vector slot) plus the string itself against the argument budget.
 *
 * @return the kernel copy, or NULL with *@err set (-EFAULT bad pointer, -E2BIG
 *         over budget).
 */
static char *exec_args_putuser(struct exec_args *ea, const char *ustr, unsigned long cost_extra, int *err)
{
	unsigned long cap;
	char *dst = ea->buf + ea->used;
	int r;

	if (ea->room <= cost_extra)
	{
		*err = -E2BIG;
		return (NULL);
	}
	cap = ea->room - cost_extra;
	if (cap > EXEC_ARG_BUF - ea->used)
		cap = EXEC_ARG_BUF - ea->used;
	r = copyinstr(ustr, dst, cap);
	if (r < 0)
	{
		*err = (r == -EFAULT) ? -EFAULT : -E2BIG;
		return (NULL);
	}
	ea->used += (unsigned long)r + 1;
	ea->room -= (unsigned long)r + 1 + cost_extra;
	return (dst);
}

/**
 * Count the entries of the NULL-terminated user vector @uvec, at most @max.
 *
 * @return the count, -EFAULT for an unreadable vector, -E2BIG past @max.
 */
static int exec_args_count(char *const *uvec, int max)
{
	int n = 0;

	if (uvec == NULL)
		return (0);
	for (;;)
	{
		const char *up;

		if (copyin(&uvec[n], &up, sizeof(up)) != 0)
			return (-EFAULT);
		if (up == NULL)
			return (n);
		if (++n > max)
			return (-E2BIG);
	}
}

/**
 * Copy the first @n strings of the user vector @uvec into @ea, storing the
 * kernel pointers in @kvec (NULL-terminated).
 *
 * @return the number copied (fewer than @n if the vector shrank meanwhile), or
 *         a negative errno.
 */
static int exec_args_copyvec(struct exec_args *ea, char *const *uvec, int n, char **kvec)
{
	int i, err = 0;

	for (i = 0; i < n; i++)
	{
		const char *up;

		if (copyin(&uvec[i], &up, sizeof(up)) != 0)
			return (-EFAULT);
		if (up == NULL)
			break;
		kvec[i] = exec_args_putuser(ea, up, sizeof(char *), &err);
		if (kvec[i] == NULL)
			return (err);
	}
	kvec[i] = NULL;
	return (i);
}

/**
 * Copy an execve's path, argv and envp out of the caller's address space.
 *
 * Everything lands in one kmalloc'd string area plus one pointer array, so it
 * survives the address-space switch.  The total is held to EXEC_ARG_MAX, the
 * same budget the loaders' stack-vector check uses: strings, their vector
 * slots and the fixed part of the vector.  Nothing is truncated; an
 * oversized argument list fails with E2BIG.  An empty argv becomes { path }.
 *
 * @return 0, or -EFAULT / -E2BIG / -ENAMETOOLONG / -ENOMEM.  On failure @ea
 *         holds nothing to free.
 */
int exec_args_copyin(struct exec_args *ea, const char *upath, char *const *uargv, char *const *uenvp)
{
	int argc, envc, r;

	memset(ea, 0, sizeof(*ea));
	argc = exec_args_count(uargv, EXEC_ARG_MAX / (int)(sizeof(char *) + 1));
	if (argc < 0)
		return (argc);
	envc = exec_args_count(uenvp, EXEC_ARG_MAX / (int)(sizeof(char *) + 1));
	if (envc < 0)
		return (envc);

	ea->buf = kmalloc(EXEC_ARG_BUF);
	/* Two spare slots ahead of argv take a "#!" interpreter and its argument. */
	ea->vec = kmalloc((u_int32_t)((2 + (unsigned long)argc + 1 + 1 + (unsigned long)envc + 1) * sizeof(char *)));
	if (ea->buf == NULL || ea->vec == NULL)
	{
		exec_args_free(ea);
		return (-ENOMEM);
	}
	ea->room = EXEC_ARG_MAX - EXEC_ARG_FIXED;

	/* The path is not marshalled onto the new stack; it uses the reserve. */
	r = copyinstr(upath, ea->buf, EXEC_PATH_MAX);
	if (r < 0)
	{
		exec_args_free(ea);
		return ((r == -EFAULT) ? -EFAULT : -ENAMETOOLONG);
	}
	ea->path = ea->buf;
	ea->used = (unsigned long)r + 1;

	ea->argv = ea->vec + 2;
	r = exec_args_copyvec(ea, uargv, argc, ea->argv);
	if (r < 0)
	{
		exec_args_free(ea);
		return (r);
	}
	ea->argc = r;
	if (ea->argc == 0)
	{
		unsigned long cost = strlen(ea->path) + 1 + sizeof(char *);

		if (cost > ea->room)
		{
			exec_args_free(ea);
			return (-E2BIG);
		}
		ea->argv[0] = ea->path;
		ea->argv[1] = NULL;
		ea->room -= cost;
		ea->argc = 1;
	}

	ea->envp = ea->argv + ea->argc + 1;
	r = exec_args_copyvec(ea, uenvp, envc, ea->envp);
	if (r < 0)
	{
		exec_args_free(ea);
		return (r);
	}
	ea->envc = r;
	return (0);
}

/**
 * Release what exec_args_copyin allocated.  Safe on a zeroed or freed @ea.
 */
void exec_args_free(struct exec_args *ea)
{
	if (ea->buf != NULL)
		kfree(ea->buf);
	if (ea->vec != NULL)
		kfree(ea->vec);
	memset(ea, 0, sizeof(*ea));
}

/**
 * Append the kernel string @str (at most @max bytes with its NUL) to @ea's
 * string area.
 *
 * @return the copy, or NULL if the area is full.
 */
static char *exec_args_putk(struct exec_args *ea, const char *str, unsigned long max)
{
	unsigned long len = strlen(str);
	char *dst = ea->buf + ea->used;

	if (len + 1 > max || len + 1 > EXEC_ARG_BUF - ea->used)
		return (NULL);
	memcpy(dst, str, len + 1);
	ea->used += len + 1;
	return (dst);
}

/**
 * If @ea->path is a "#!" script, rewrite @ea to run its interpreter.
 *
 * "#!" [blanks] interpreter [blanks argument] newline: everything after the
 * interpreter (trailing blanks trimmed) is one argument, as on FreeBSD.  The
 * argv becomes: interpreter, [argument], path, argv[1], argv[2], ... and
 * @ea->path becomes the interpreter.  One level only; the interpreter must be
 * an ELF program.
 *
 * @return 1 if rewritten, 0 if @ea->path is not a script (or unreadable),
 *         -E2BIG if the longer argv no longer fits the budget.
 */
int exec_args_script(struct exec_args *ea)
{
	fileDescriptor_t *fd;
	char line[256], interp[256], iarg[256];
	unsigned long cost;
	char *kinterp, *kiarg = NULL;
	size_t n, i = 2, k;

	fd = fopen(ea->path, "r");
	if (fd == NULL)
		return (0);
	n = fread(line, 1, sizeof(line) - 1, fd);
	fclose(fd);
	if ((ssize_t)n < 3 || line[0] != '#' || line[1] != '!')
		return (0);
	line[n] = '\0';

	while (line[i] == ' ' || line[i] == '\t')
		i++;
	for (k = 0; line[i] != '\0' && line[i] != ' ' && line[i] != '\t' && line[i] != '\n'; i++)
		interp[k++] = line[i];
	interp[k] = '\0';
	if (k == 0)
		return (0);
	while (line[i] == ' ' || line[i] == '\t')
		i++;
	for (k = 0; line[i] != '\0' && line[i] != '\n'; i++)
		iarg[k++] = line[i];
	while (k > 0 && (iarg[k - 1] == ' ' || iarg[k - 1] == '\t' || iarg[k - 1] == '\r'))
		k--;
	iarg[k] = '\0';

	/* New strings: interpreter (+ argument), and path in place of argv[0]. */
	cost = strlen(interp) + 1 + sizeof(char *) + strlen(ea->path) + 1;
	if (iarg[0] != '\0')
		cost += strlen(iarg) + 1 + sizeof(char *);
	if (cost > ea->room)
		return (-E2BIG);
	kinterp = exec_args_putk(ea, interp, sizeof(interp));
	if (iarg[0] != '\0')
		kiarg = exec_args_putk(ea, iarg, sizeof(iarg));
	if (kinterp == NULL || (iarg[0] != '\0' && kiarg == NULL))
		return (-E2BIG);
	ea->room -= cost;

	ea->argv[0] = ea->path;
	if (kiarg != NULL)
	{
		*--ea->argv = kiarg;
		ea->argc++;
	}
	*--ea->argv = kinterp;
	ea->argc++;
	ea->path = kinterp;
	return (1);
}
