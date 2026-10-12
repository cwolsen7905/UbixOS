/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * copyin/copyout: the one place a syscall moves bytes across the user/kernel
 * boundary (kernel-rules K-40/K-41).  Machine-independent; the per-arch parts
 * are VMM_USER_START/VMM_USER_END (<machine/vmm_layout.h>) and
 * md_user_page_kva().  Lengths are unsigned long: the kernel's size_t is a
 * 32-bit int.
 */
#ifndef _UBIXOS_UACCESS_H
#define _UBIXOS_UACCESS_H

#include <sys/types.h>

int copyin(const void *uaddr, void *kaddr, unsigned long len);
int copyout(const void *kaddr, void *uaddr, unsigned long len);
int copyinstr(const void *uaddr, char *kaddr, unsigned long maxlen);

#endif /* _UBIXOS_UACCESS_H */
