/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * ubix_pool_query — userland thunk for the UbixFS pool/dataset query (native
 * syscall 67).  Backs the in-OS `ubpool` and `ubfs` commands.  See
 * <api/ubfs_pool.h> for the record layout and docs/design/ubixfs-pool-plan.md.
 */
#include <sys/ubix_syscall.h>

UBIX_NATIVE_THUNK(ubix_pool_query, 67);
