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
#include <ctype.h>
#include "db.h"
#include "persist.h"

static int g_dirty = 0;

void persist_mark_dirty(void)
{
	g_dirty = 1;
}

int persist_dirty(void)
{
	return (g_dirty);
}

/**
 * Strip leading and trailing whitespace in place; returns the trimmed start.
 */
static char *trim(char *s)
{
	char *end;

	while (*s == ' ' || *s == '\t')
		s++;
	end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r' || end[-1] == '\n'))
		*--end = '\0';
	return (s);
}

/**
 * Classify and unquote a value token: a "quoted" token is a string, true/false
 * a bool, an all-digit (optionally signed) token an int, everything else a
 * string.  The cleaned value text is written to out.
 */
static void classify_value(char *val, ub_type_t *type, char *out, int outlen)
{
	if (val[0] == '"')
	{
		char *end = strrchr(val, '"');
		*type = UB_STR;
		if (end != NULL && end != val)
		{
			int n = (int)(end - val - 1);
			if (n >= outlen)
				n = outlen - 1;
			memcpy(out, val + 1, (size_t)n);
			out[n] = '\0';
		}
		else
		{
			snprintf(out, (size_t)outlen, "%s", val + 1);
		}
		return;
	}

	if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0)
	{
		*type = UB_BOOL;
		snprintf(out, (size_t)outlen, "%s", val);
		return;
	}

	{
		char *p = val;
		int isint = (*p != '\0');
		if (*p == '-' || *p == '+')
			p++;
		if (*p == '\0')
			isint = 0;
		for (; *p != '\0'; p++)
			if (!isdigit((unsigned char)*p))
			{
				isint = 0;
				break;
			}
		*type = isint ? UB_INT : UB_STR;
		snprintf(out, (size_t)outlen, "%s", val);
	}
}

int persist_load(const char *path)
{
	FILE *f = fopen(path, "r");
	char line[UB_PATH_MAX + UB_VAL_MAX + 16];

	if (f == NULL)
		return (-1);

	while (fgets(line, sizeof(line), f) != NULL)
	{
		char *eq, *key, *val, *vclean;
		char valbuf[UB_VAL_MAX];
		ub_type_t type;

		key = trim(line);
		if (key[0] == '\0' || key[0] == '#' || key[0] == ';')
			continue;

		eq = strchr(key, '=');
		if (eq == NULL)
			continue;
		*eq = '\0';
		key = trim(key);
		val = trim(eq + 1);
		if (key[0] != '/' || val[0] == '\0')
			continue;

		vclean = valbuf;
		classify_value(val, &type, vclean, (int)sizeof(valbuf));
		ub_set(key, type, vclean);
	}

	fclose(f);
	return (0);
}

/**
 * DFS-emit a node: containers recurse into children, leaves write one line.
 * parent_path is the absolute path of this node's parent ("" for root).
 */
static void emit_node(FILE *f, struct ub_node *n, const char *parent_path)
{
	char full[UB_PATH_MAX * 2];

	snprintf(full, sizeof(full), "%s/%s", parent_path, n->name);

	switch (n->type)
	{
		case UB_CONTAINER:
		{
			struct ub_node *c;
			for (c = n->child; c != NULL; c = c->sibling)
				emit_node(f, c, full);
			break;
		}
		case UB_STR:
			fprintf(f, "%s = \"%s\"\n", full, n->sval);
			break;
		case UB_INT:
			fprintf(f, "%s = %d\n", full, n->ival);
			break;
		case UB_BOOL:
			fprintf(f, "%s = %s\n", full, n->ival ? "true" : "false");
			break;
		default:
			break;
	}
}

int persist_save(const char *path)
{
	FILE *f = fopen(path, "w");
	struct ub_node *root = ub_root();
	struct ub_node *c;

	if (f == NULL)
		return (-1);

	for (c = root->child; c != NULL; c = c->sibling)
		emit_node(f, c, "");

	fclose(f);
	g_dirty = 0;
	return (0);
}
