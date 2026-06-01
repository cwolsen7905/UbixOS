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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "db.h"

static struct ub_node g_root;
static int g_root_init = 0;

/**
 * Return the registry root container, initialising it on first use.
 */
struct ub_node *ub_root(void)
{
	if (!g_root_init)
	{
		memset(&g_root, 0, sizeof(g_root));
		g_root.type = UB_CONTAINER;
		g_root_init = 1;
	}
	return (&g_root);
}

/**
 * Render a node's scalar value as text into out.
 */
static void node_value_text(const struct ub_node *n, char *out, int len)
{
	if (len <= 0)
		return;
	switch (n->type)
	{
		case UB_STR:
			snprintf(out, (size_t)len, "%s", n->sval);
			break;
		case UB_INT:
			snprintf(out, (size_t)len, "%d", n->ival);
			break;
		case UB_BOOL:
			snprintf(out, (size_t)len, "%s", n->ival ? "true" : "false");
			break;
		default:
			out[0] = '\0';
			break;
	}
}

/**
 * Find a direct child of parent by name.
 */
static struct ub_node *find_child(struct ub_node *parent, const char *name)
{
	struct ub_node *c;

	for (c = parent->child; c != NULL; c = c->sibling)
		if (strcmp(c->name, name) == 0)
			return (c);
	return (NULL);
}

/**
 * Append a new empty container child to parent (preserving insertion order).
 */
static struct ub_node *add_child(struct ub_node *parent, const char *name)
{
	struct ub_node *n = (struct ub_node *)calloc(1, sizeof(struct ub_node));

	if (n == NULL)
		return (NULL);
	snprintf(n->name, sizeof(n->name), "%s", name);
	n->type = UB_CONTAINER;
	n->parent = parent;

	if (parent->child == NULL)
	{
		parent->child = n;
	}
	else
	{
		struct ub_node *t = parent->child;
		while (t->sibling != NULL)
			t = t->sibling;
		t->sibling = n;
	}
	return (n);
}

/**
 * Walk an absolute path from the root, optionally creating intermediate
 * containers.  "/" resolves to the root.
 */
static struct ub_node *walk(const char *path, int create)
{
	struct ub_node *cur = ub_root();
	char buf[UB_PATH_MAX * 2];
	char *save = NULL;
	char *tok;

	snprintf(buf, sizeof(buf), "%s", path);
	for (tok = strtok_r(buf, "/", &save); tok != NULL; tok = strtok_r(NULL, "/", &save))
	{
		struct ub_node *next = find_child(cur, tok);
		if (next == NULL)
		{
			if (!create)
				return (NULL);
			next = add_child(cur, tok);
			if (next == NULL)
				return (NULL);
		}
		cur = next;
	}
	return (cur);
}

struct ub_node *ub_find(const char *path)
{
	return (walk(path, 0));
}

struct ub_node *ub_find_or_create(const char *path)
{
	return (walk(path, 1));
}

int ub_set(const char *path, ub_type_t type, const char *value)
{
	struct ub_node *n = ub_find_or_create(path);

	if (n == NULL)
		return (-1);

	n->type = type;
	switch (type)
	{
		case UB_STR:
			snprintf(n->sval, sizeof(n->sval), "%s", value ? value : "");
			n->ival = 0;
			break;
		case UB_INT:
			n->ival = value ? atoi(value) : 0;
			n->sval[0] = '\0';
			break;
		case UB_BOOL:
			n->ival = (value && (value[0] == 't' || value[0] == 'T' || value[0] == '1')) ? 1 : 0;
			n->sval[0] = '\0';
			break;
		default:
			break;
	}
	return (0);
}

int ub_get(const char *path, ub_type_t *type, char *out, int len)
{
	struct ub_node *n = ub_find(path);

	if (n == NULL)
		return (-1);
	if (type != NULL)
		*type = n->type;
	node_value_text(n, out, len);
	return (0);
}

int ub_enum(const char *path, char *names, int len, int *truncated)
{
	struct ub_node *n = ub_find(path);
	struct ub_node *c;
	int count = 0;
	int pos = 0;
	int stopped = 0;

	if (truncated != NULL)
		*truncated = 0;
	if (len > 0)
		names[0] = '\0';
	if (n == NULL)
		return (-1);

	for (c = n->child; c != NULL; c = c->sibling)
	{
		if (!stopped)
		{
			int nl = (int)strlen(c->name);
			int need = (count > 0 ? 1 : 0) + nl;
			if (pos + need + 1 > len)
			{
				stopped = 1;
				if (truncated != NULL)
					*truncated = 1;
			}
			else
			{
				if (count > 0)
					names[pos++] = '\n';
				memcpy(names + pos, c->name, (size_t)nl);
				pos += nl;
				names[pos] = '\0';
			}
		}
		count++;
	}
	return (count);
}

/**
 * Recursively free a node and all its descendants.
 */
static void free_subtree(struct ub_node *n)
{
	struct ub_node *c = n->child;

	while (c != NULL)
	{
		struct ub_node *next = c->sibling;
		free_subtree(c);
		c = next;
	}
	free(n);
}

int ub_delete(const char *path)
{
	struct ub_node *n = ub_find(path);
	struct ub_node *p;

	if (n == NULL || n == ub_root())
		return (-1);

	p = n->parent;
	if (p->child == n)
	{
		p->child = n->sibling;
	}
	else
	{
		struct ub_node *t = p->child;
		while (t != NULL && t->sibling != n)
			t = t->sibling;
		if (t != NULL)
			t->sibling = n->sibling;
	}
	free_subtree(n);
	return (0);
}
