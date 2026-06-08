/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 *
 * UbixFS directory helper — a name→object map stored in a DMU object.  Shared by
 * the DSL (the MOS dataset directory) and ubfs_fs (filesystem directories).
 *
 * v1 on-disk layout in the directory object's data: [uint64_t count][entries].
 * A removed entry has obj == 0 (count is the high-water slot count, it does not
 * shrink in v1).  A scalable hash directory is a later phase.
 */
#ifndef _UBIXFS_DIR_H
#define _UBIXFS_DIR_H

#include "ubfs_dmu.h"

/** Look up `name` in directory object `dirobj`; on success sets *obj (and *type
 *  if non-NULL).  @return 0 if found, negative if not. */
int ubfs_dir_lookup(ubfs_dmu_os_t *os, uint64_t dirobj, const char *name, uint64_t *obj, uint8_t *type);

/** Add an entry (`name` → `obj`, of `type`) to directory `dirobj`. */
int ubfs_dir_add(ubfs_dmu_os_t *os, uint64_t dirobj, const char *name, uint64_t obj, uint8_t type);

/** Remove `name` from directory `dirobj`.  @return 0 or negative if absent. */
int ubfs_dir_remove(ubfs_dmu_os_t *os, uint64_t dirobj, const char *name);

/** Iterate live entries.  `cb` returns non-zero to stop early (that value is
 *  returned).  @return 0 on full iteration, or the cb's stop value, or negative. */
typedef int (*ubfs_dir_cb)(void *arg, const char *name, uint64_t obj, uint8_t type);
int ubfs_dir_foreach(ubfs_dmu_os_t *os, uint64_t dirobj, ubfs_dir_cb cb, void *arg);

/** Number of live entries (for emptiness checks). */
int ubfs_dir_count(ubfs_dmu_os_t *os, uint64_t dirobj, uint64_t *count_out);

#endif /* _UBIXFS_DIR_H */
