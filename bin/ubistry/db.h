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

/*
 * ubistry daemon-internal store: an n-ary tree of typed nodes addressed by
 * path.  Children are kept in insertion order via the child/sibling links so
 * that ordered records (e.g. start-menu items 0,1,2) enumerate predictably.
 */

#ifndef _UBISTRY_DB_H
#define _UBISTRY_DB_H

#include <ubistry/ubistry.h>

struct ub_node
{
	char name[UB_NAME_MAX];
	ub_type_t type;
	char sval[192]; /* UB_STR text */
	int ival;       /* UB_INT / UB_BOOL */
	struct ub_node *parent;
	struct ub_node *child;   /* first child */
	struct ub_node *sibling; /* next sibling (insertion order) */
};

/* Tree operations.  Paths are absolute ("/a/b/c"); "/" is the root. */
struct ub_node *ub_root(void);
struct ub_node *ub_find(const char *path);
struct ub_node *ub_find_or_create(const char *path);

/* Create/update a leaf value (value given as text, parsed per type). */
int ub_set(const char *path, ub_type_t type, const char *value);

/* Read a leaf value back as text.  @return 0 on success, -1 if missing. */
int ub_get(const char *path, ub_type_t *type, char *out, int len);

/*
 * List child node names of a container into names ('\n'-separated).
 * @return child count, or -1 if the path is missing.  *truncated is set when
 * not every name fit.
 */
int ub_enum(const char *path, char *names, int len, int *truncated);

/* Delete a node and its subtree.  @return 0 on success, -1 if missing. */
int ub_delete(const char *path);

#endif /* _UBISTRY_DB_H */
