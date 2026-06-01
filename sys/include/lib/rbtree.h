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

#ifndef _LIB_RBTREE_H_
#define _LIB_RBTREE_H_

#include <sys/types.h>

/*
 * Intrusive red-black tree.
 *
 * Embed struct rb_node as the first member of the container struct so that
 * a simple cast converts between rb_node * and container *.  Alternatively
 * use rb_entry() for non-first-member placement.
 *
 *   struct vm_map_entry {
 *       struct rb_node rb;   // must be first, or use rb_entry()
 *       uintptr_t      vm_start;
 *       ...
 *   };
 */

#define RB_RED   0
#define RB_BLACK 1

struct rb_node {
	struct rb_node *rb_left;
	struct rb_node *rb_right;
	struct rb_node *rb_parent;
	int             rb_color;
};

struct rb_root {
	struct rb_node *rb_node;
};

#define RB_ROOT_INIT  { NULL }

/* Cast a rb_node * back to the enclosing struct. */
#define rb_entry(ptr, type, member) \
	((type *)((char *)(ptr) - __builtin_offsetof(type, member)))

/*
 * rb_insert — insert @node into @root.
 * @cmp(a, b) returns <0 if a goes left of b, >0 if right, 0 if equal.
 * Duplicate keys are inserted to the right (no replacement).
 */
void rb_insert(struct rb_root *root, struct rb_node *node,
    int (*cmp)(const struct rb_node *, const struct rb_node *));

/*
 * rb_erase — remove @node from @root.
 * The caller is responsible for freeing the container.
 */
void rb_erase(struct rb_root *root, struct rb_node *node);

/*
 * rb_find — locate a node by key using @cmp_key.
 * @cmp_key(node, key) returns 0 if @key falls inside @node's range,
 * >0 if @key is to the left of @node (go left), <0 if to the right.
 */
struct rb_node *rb_find(struct rb_root *root, uintptr_t key,
    int (*cmp_key)(const struct rb_node *, uintptr_t));

/* rb_first — return the leftmost (smallest key) node, or NULL. */
struct rb_node *rb_first(const struct rb_root *root);

/* rb_next — in-order successor of @node, or NULL. */
struct rb_node *rb_next(const struct rb_node *node);

#endif /* _LIB_RBTREE_H_ */
