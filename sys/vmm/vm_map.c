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

#include <vmm/vm_map.h>
#include <lib/kmalloc.h>

/* ------------------------------------------------------------------ */
/* RB tree comparators                                                  */
/* ------------------------------------------------------------------ */

static int vma_cmp(const struct rb_node *a, const struct rb_node *b)
{
	const vm_map_entry_t *ea = (const vm_map_entry_t *)a;
	const vm_map_entry_t *eb = (const vm_map_entry_t *)b;

	if (ea->vm_start < eb->vm_start)
	{
		return (-1);
	}
	if (ea->vm_start > eb->vm_start)
	{
		return (1);
	}
	return (0);
}

/*
 * cmp_key: returns >0 if addr is left of node (go left),
 *          <0 if addr is right of node (go right), 0 if inside.
 */
static int vma_cmp_key(const struct rb_node *n, uintptr_t addr)
{
	const vm_map_entry_t *e = (const vm_map_entry_t *)n;

	if (addr < e->vm_start)
	{
		return (1);
	}
	if (addr >= e->vm_end)
	{
		return (-1);
	}
	return (0);
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

int vm_map_insert(vm_map_t *map, uintptr_t start, uintptr_t end, u_int32_t prot, u_int32_t flags)
{
	vm_map_entry_t *e = kmalloc(sizeof *e);

	if (e == NULL)
	{
		return (-1);
	}

	e->vm_start = start;
	e->vm_end = end;
	e->vm_prot = prot;
	e->vm_flags = flags;
	e->vm_vnode = NULL;
	e->vm_offset = 0;

	rb_insert(&map->vm_root, &e->rb, vma_cmp);
	map->vm_nentries++;
	return (0);
}

void vm_map_remove(vm_map_t *map, uintptr_t start, uintptr_t end)
{
	struct rb_node *n = rb_first(&map->vm_root);

	while (n != NULL)
	{
		vm_map_entry_t *e = (vm_map_entry_t *)n;
		struct rb_node *next = rb_next(n);

		/* No overlap — entries are sorted; once we pass end we're done. */
		if (e->vm_start >= end)
		{
			break;
		}

		/* No overlap on the left. */
		if (e->vm_end <= start)
		{
			n = next;
			continue;
		}

		/* Full containment: remove and free. */
		if (e->vm_start >= start && e->vm_end <= end)
		{
			rb_erase(&map->vm_root, n);
			map->vm_nentries--;
			kfree(e);
			n = next;
			continue;
		}

		/* Partial overlap — left tail: trim vm_end. */
		if (e->vm_start < start && e->vm_end > start && e->vm_end <= end)
		{
			e->vm_end = start;
			n = next;
			continue;
		}

		/* Partial overlap — right tail: trim vm_start. */
		if (e->vm_start >= start && e->vm_start < end && e->vm_end > end)
		{
			e->vm_start = end;
			n = next;
			continue;
		}

		/* Entry fully contains [start, end): split into two. */
		if (e->vm_start < start && e->vm_end > end)
		{
			vm_map_entry_t *tail = kmalloc(sizeof *tail);
			if (tail != NULL)
			{
				*tail = *e;
				tail->vm_start = end;
				rb_insert(&map->vm_root, &tail->rb, vma_cmp);
				map->vm_nentries++;
			}
			e->vm_end = start;
			n = next;
			continue;
		}

		n = next;
	}
}

vm_map_entry_t *vm_map_lookup(vm_map_t *map, uintptr_t addr)
{
	struct rb_node *n = rb_find(&map->vm_root, addr, vma_cmp_key);

	return ((vm_map_entry_t *)n);
}

void vm_map_free(vm_map_t *map)
{
	struct rb_node *n = rb_first(&map->vm_root);

	while (n != NULL)
	{
		struct rb_node *next = rb_next(n);
		rb_erase(&map->vm_root, n);
		kfree((vm_map_entry_t *)n);
		n = next;
	}
	map->vm_nentries = 0;
}

int vm_map_copy(vm_map_t *dst, const vm_map_t *src)
{
	struct rb_node *n;

	for (n = rb_first(&src->vm_root); n != NULL; n = rb_next(n))
	{
		const vm_map_entry_t *se = (const vm_map_entry_t *)n;
		vm_map_entry_t *de = kmalloc(sizeof *de);

		if (de == NULL)
		{
			return (-1);
		}

		*de = *se;
		rb_insert(&dst->vm_root, &de->rb, vma_cmp);
		dst->vm_nentries++;
	}
	return (0);
}
