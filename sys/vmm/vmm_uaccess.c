/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * copyin/copyout for the 64-bit ports (aarch64 + x86_64).
 *
 * A user range is accepted only if it lies inside [VMM_USER_START,
 * VMM_USER_END] and every page of it is mapped in the current task.  Bytes move
 * through each page's kernel alias (md_user_page_kva), so a bad pointer is an
 * -EFAULT return, never a kernel fault.  A page of a valid mapping that is not
 * yet present (demand-paged text/data, untouched anonymous memory) is faulted
 * in first, as the trap handler would.  Page permissions are not checked: a
 * copyout to a read-only user page succeeds (no copy-on-write exists, so no
 * other task can see it).
 */

#include <sys/types.h>
#include <sys/errno.h>
#include <ubixos/uaccess.h>
#include <vmm/vmm.h>
#include <vmm/vmm_shmap.h>
#include <vmm/vm_map.h> /* vmm_demand_fault */
#include <string.h>

/**
 * Check that [@uaddr, @uaddr + @len) is inside the user VA range.
 *
 * @return 1 if it is (or @len is 0), 0 otherwise.
 */
static int uaccess_range_ok(uintptr_t uaddr, unsigned long len)
{
	if (len == 0)
		return (1);
	if (uaddr < VMM_USER_START || uaddr > VMM_USER_END)
		return (0);
	return (len - 1 <= VMM_USER_END - uaddr);
}

/**
 * Kernel alias of the current task's user page at @ua, faulting it in first if
 * it belongs to a mapping but is not present yet.
 *
 * @return the kernel pointer for @ua, or NULL if no mapping covers it.
 */
static void *uaccess_page_kva(uintptr_t ua)
{
	void *kva = md_user_page_kva(ua);
	u_int64_t *root;

	if (kva != NULL)
		return (kva);
	root = md_user_aspace_root();
	if (root == NULL || vmm_demand_fault(root, ua) != 0)
		return (NULL);
	return (md_user_page_kva(ua));
}

/**
 * Copy @len bytes between the kernel buffer @kbuf and user VA @uaddr, one user
 * page at a time.  @to_user selects the direction.
 *
 * @return 0 on success, -EFAULT if any part of the user range is outside user
 *         space or unmapped (bytes before the bad page may have been copied).
 */
static int uaccess_copy(uintptr_t uaddr, void *kbuf, unsigned long len, int to_user)
{
	u_int8_t *k = kbuf;

	if (!uaccess_range_ok(uaddr, len))
		return (-EFAULT);
	while (len > 0)
	{
		unsigned long chunk = PAGE_SIZE - (uaddr & (PAGE_SIZE - 1));
		void *kva;

		if (chunk > len)
			chunk = len;
		kva = uaccess_page_kva(uaddr);
		if (kva == NULL)
			return (-EFAULT);
		if (to_user)
			memcpy(kva, k, (size_t)chunk); /* chunk <= PAGE_SIZE */
		else
			memcpy(k, kva, (size_t)chunk);
		uaddr += chunk;
		k += chunk;
		len -= chunk;
	}
	return (0);
}

/**
 * Copy @len bytes from the current task's user address @uaddr into @kaddr.
 *
 * @return 0 on success, -EFAULT on a bad user range.
 */
int copyin(const void *uaddr, void *kaddr, unsigned long len)
{
	return (uaccess_copy((uintptr_t)uaddr, kaddr, len, 0));
}

/**
 * Copy @len bytes from @kaddr out to the current task's user address @uaddr.
 *
 * @return 0 on success, -EFAULT on a bad user range.
 */
int copyout(const void *kaddr, void *uaddr, unsigned long len)
{
	return (uaccess_copy((uintptr_t)uaddr, (void *)(uintptr_t)kaddr, len, 1));
}

/**
 * Copy a NUL-terminated string of at most @maxlen bytes (terminator included)
 * from the current task's user address @uaddr into @kaddr.
 *
 * @return the string length (excluding the NUL) on success, -EFAULT on a bad
 *         user range, -ENAMETOOLONG if no NUL lies within @maxlen bytes.
 */
int copyinstr(const void *uaddr, char *kaddr, unsigned long maxlen)
{
	uintptr_t ua = (uintptr_t)uaddr;
	unsigned long i;

	for (i = 0; i < maxlen; i++, ua++)
	{
		const char *kva;

		if (!uaccess_range_ok(ua, 1))
			return (-EFAULT);
		kva = uaccess_page_kva(ua);
		if (kva == NULL)
			return (-EFAULT);
		kaddr[i] = *kva;
		if (kaddr[i] == '\0')
			return ((int)i);
	}
	if (maxlen > 0)
		kaddr[maxlen - 1] = '\0';
	return (-ENAMETOOLONG);
}
