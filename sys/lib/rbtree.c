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

#include <lib/rbtree.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static void
rotate_left(struct rb_root *root, struct rb_node *x)
{
	struct rb_node *y = x->rb_right;

	x->rb_right = y->rb_left;
	if (y->rb_left)
		y->rb_left->rb_parent = x;

	y->rb_parent = x->rb_parent;
	if (!x->rb_parent)
		root->rb_node = y;
	else if (x == x->rb_parent->rb_left)
		x->rb_parent->rb_left = y;
	else
		x->rb_parent->rb_right = y;

	y->rb_left = x;
	x->rb_parent = y;
}

static void
rotate_right(struct rb_root *root, struct rb_node *y)
{
	struct rb_node *x = y->rb_left;

	y->rb_left = x->rb_right;
	if (x->rb_right)
		x->rb_right->rb_parent = y;

	x->rb_parent = y->rb_parent;
	if (!y->rb_parent)
		root->rb_node = x;
	else if (y == y->rb_parent->rb_left)
		y->rb_parent->rb_left = x;
	else
		y->rb_parent->rb_right = x;

	x->rb_right = y;
	y->rb_parent = x;
}

static void
insert_fixup(struct rb_root *root, struct rb_node *z)
{
	while (z->rb_parent && z->rb_parent->rb_color == RB_RED) {
		struct rb_node *p = z->rb_parent;
		struct rb_node *g = p->rb_parent;

		if (!g)
			break;

		if (p == g->rb_left) {
			struct rb_node *u = g->rb_right;
			if (u && u->rb_color == RB_RED) {
				/* Case 1: uncle red — recolour and move up. */
				p->rb_color = RB_BLACK;
				u->rb_color = RB_BLACK;
				g->rb_color = RB_RED;
				z = g;
			} else {
				if (z == p->rb_right) {
					/* Case 2: uncle black, z right child — rotate left. */
					z = p;
					rotate_left(root, z);
					p = z->rb_parent;
					g = p->rb_parent;
				}
				/* Case 3: uncle black, z left child — rotate right. */
				p->rb_color = RB_BLACK;
				g->rb_color = RB_RED;
				rotate_right(root, g);
			}
		} else {
			/* Mirror of above. */
			struct rb_node *u = g->rb_left;
			if (u && u->rb_color == RB_RED) {
				p->rb_color = RB_BLACK;
				u->rb_color = RB_BLACK;
				g->rb_color = RB_RED;
				z = g;
			} else {
				if (z == p->rb_left) {
					z = p;
					rotate_right(root, z);
					p = z->rb_parent;
					g = p->rb_parent;
				}
				p->rb_color = RB_BLACK;
				g->rb_color = RB_RED;
				rotate_left(root, g);
			}
		}
	}
	root->rb_node->rb_color = RB_BLACK;
}

/*
 * Replace subtree rooted at @u with subtree rooted at @v.
 * Does NOT update v->rb_parent when v is NULL — callers handle that.
 */
static void
transplant(struct rb_root *root, struct rb_node *u, struct rb_node *v)
{
	if (!u->rb_parent)
		root->rb_node = v;
	else if (u == u->rb_parent->rb_left)
		u->rb_parent->rb_left = v;
	else
		u->rb_parent->rb_right = v;
	if (v)
		v->rb_parent = u->rb_parent;
}

static struct rb_node *
subtree_min(struct rb_node *x)
{
	while (x->rb_left)
		x = x->rb_left;
	return x;
}

static void
erase_fixup(struct rb_root *root, struct rb_node *x, struct rb_node *xp)
{
	while (x != root->rb_node && (!x || x->rb_color == RB_BLACK)) {
		if (x == (xp ? xp->rb_left : NULL)) {
			struct rb_node *w = xp->rb_right;
			if (w && w->rb_color == RB_RED) {
				/* Case 1: sibling red. */
				w->rb_color = RB_BLACK;
				xp->rb_color = RB_RED;
				rotate_left(root, xp);
				w = xp->rb_right;
			}
			if ((!w->rb_left  || w->rb_left->rb_color  == RB_BLACK) &&
			    (!w->rb_right || w->rb_right->rb_color == RB_BLACK)) {
				/* Case 2: sibling black, both nephews black. */
				if (w) w->rb_color = RB_RED;
				x  = xp;
				xp = x->rb_parent;
			} else {
				if (!w->rb_right || w->rb_right->rb_color == RB_BLACK) {
					/* Case 3: sibling black, left nephew red. */
					if (w->rb_left) w->rb_left->rb_color = RB_BLACK;
					if (w) w->rb_color = RB_RED;
					rotate_right(root, w);
					w = xp->rb_right;
				}
				/* Case 4: sibling black, right nephew red. */
				if (w) w->rb_color = xp->rb_color;
				xp->rb_color = RB_BLACK;
				if (w && w->rb_right) w->rb_right->rb_color = RB_BLACK;
				rotate_left(root, xp);
				x  = root->rb_node;
				xp = NULL;
			}
		} else {
			/* Mirror. */
			struct rb_node *w = xp->rb_left;
			if (w && w->rb_color == RB_RED) {
				w->rb_color = RB_BLACK;
				xp->rb_color = RB_RED;
				rotate_right(root, xp);
				w = xp->rb_left;
			}
			if ((!w->rb_right || w->rb_right->rb_color == RB_BLACK) &&
			    (!w->rb_left  || w->rb_left->rb_color  == RB_BLACK)) {
				if (w) w->rb_color = RB_RED;
				x  = xp;
				xp = x->rb_parent;
			} else {
				if (!w->rb_left || w->rb_left->rb_color == RB_BLACK) {
					if (w->rb_right) w->rb_right->rb_color = RB_BLACK;
					if (w) w->rb_color = RB_RED;
					rotate_left(root, w);
					w = xp->rb_left;
				}
				if (w) w->rb_color = xp->rb_color;
				xp->rb_color = RB_BLACK;
				if (w && w->rb_left) w->rb_left->rb_color = RB_BLACK;
				rotate_right(root, xp);
				x  = root->rb_node;
				xp = NULL;
			}
		}
	}
	if (x) x->rb_color = RB_BLACK;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void
rb_insert(struct rb_root *root, struct rb_node *node,
    int (*cmp)(const struct rb_node *, const struct rb_node *))
{
	struct rb_node **p      = &root->rb_node;
	struct rb_node  *parent = NULL;

	while (*p) {
		parent = *p;
		if (cmp(node, *p) < 0)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	node->rb_left = node->rb_right = NULL;
	node->rb_parent = parent;
	node->rb_color  = RB_RED;
	*p = node;

	insert_fixup(root, node);
}

void
rb_erase(struct rb_root *root, struct rb_node *z)
{
	struct rb_node *x, *xp;
	struct rb_node *y          = z;
	int             y_orig_col = y->rb_color;

	if (!z->rb_left) {
		x  = z->rb_right;
		xp = z->rb_parent;
		transplant(root, z, z->rb_right);
	} else if (!z->rb_right) {
		x  = z->rb_left;
		xp = z->rb_parent;
		transplant(root, z, z->rb_left);
	} else {
		y          = subtree_min(z->rb_right);
		y_orig_col = y->rb_color;
		x          = y->rb_right;

		if (y->rb_parent == z) {
			xp = y;
		} else {
			xp = y->rb_parent;
			transplant(root, y, y->rb_right);
			y->rb_right          = z->rb_right;
			y->rb_right->rb_parent = y;
		}
		transplant(root, z, y);
		y->rb_left           = z->rb_left;
		y->rb_left->rb_parent  = y;
		y->rb_color          = z->rb_color;
	}

	if (y_orig_col == RB_BLACK)
		erase_fixup(root, x, xp);
}

struct rb_node *
rb_find(struct rb_root *root, uintptr_t key,
    int (*cmp_key)(const struct rb_node *, uintptr_t))
{
	struct rb_node *n = root->rb_node;

	while (n) {
		int c = cmp_key(n, key);
		if (c > 0)
			n = n->rb_left;
		else if (c < 0)
			n = n->rb_right;
		else
			return n;
	}
	return NULL;
}

struct rb_node *
rb_first(const struct rb_root *root)
{
	struct rb_node *n = root->rb_node;

	if (!n)
		return NULL;
	while (n->rb_left)
		n = n->rb_left;
	return n;
}

struct rb_node *
rb_next(const struct rb_node *node)
{
	struct rb_node *n;

	if (node->rb_right) {
		n = node->rb_right;
		while (n->rb_left)
			n = n->rb_left;
		return n;
	}
	n = node->rb_parent;
	while (n && node == n->rb_right) {
		node = n;
		n    = n->rb_parent;
	}
	return n;
}
