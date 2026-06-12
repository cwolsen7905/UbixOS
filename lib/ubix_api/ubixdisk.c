/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubix_disk_query — userland thunk for block-device / partition enumeration
 * (native syscall 68).  Backs the Disk Utility app + a future `diskutil` CLI.
 * See <api/ubix_disk.h> for the record layout and
 * docs/design/disk-utility-plan.md.
 */
#include <sys/ubix_syscall.h>

UBIX_NATIVE_THUNK(ubix_disk_query, 68);
