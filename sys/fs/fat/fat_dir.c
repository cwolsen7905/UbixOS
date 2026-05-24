/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1) Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors.
 * 2) Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, the following disclaimer and the list of authors
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3) Neither the name of the UbixOS Project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include <sys/bus.h>
#include <lib/kprintf.h>
#include <lib/kmalloc.h>
#include <string.h>
#include <fs/fat/fat_dir.h>
#include <fs/fat/fat_sector.h>
#include <fs/fat/fat_clust.h>

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */

static u_int16_t
le16(const u_int8_t *p)
{
	return ((u_int16_t)p[0] | ((u_int16_t)p[1] << 8));
}

static void
put_le32(u_int8_t *p, u_int32_t v)
{
	p[0] = (u_int8_t)(v);
	p[1] = (u_int8_t)(v >> 8);
	p[2] = (u_int8_t)(v >> 16);
	p[3] = (u_int8_t)(v >> 24);
}

static char
fat_upcase(char c)
{
	return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c;
}

static int
fat_names_equal(const char *a, const char *b)
{
	while (*a && *b) {
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return (0);
		a++;
		b++;
	}
	return (*a == '\0' && *b == '\0');
}

/* ------------------------------------------------------------------ */
/* SFN helpers                                                         */
/* ------------------------------------------------------------------ */

/*
 * Compute the LFN checksum over the 11-byte SFN field.
 * (right-rotate accumulator, add next byte)
 */
static u_int8_t
sfn_checksum(const u_int8_t *sfn)
{
	u_int8_t	 sum = 0;
	int	 i;

	for (i = 11; i != 0; i--)
		sum = (u_int8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + *sfn++);
	return (sum);
}

/*
 * Convert an 11-byte SFN (no dot, space-padded) to a NUL-terminated string.
 * e.g. "FILE    TXT" -> "FILE.TXT", "DIR        " -> "DIR"
 *
 * sfn points to the full 32-byte raw directory entry so we can read the
 * NT_RES byte at sfn[12]: bit 3 = basename lowercase, bit 4 = ext lowercase.
 * mtools sets these flags when creating files with lowercase names; without
 * them, tcsh would hash "LS" but look up "ls" and always say "not found".
 */
static void
sfn_to_name(const u_int8_t *sfn, char *out)
{
	int	 i, len = 0;
	u_int8_t	 nt = sfn[12]; /* NT_RES: 0x08 = lc base, 0x10 = lc ext */

	/* Base name: strip trailing spaces */
	for (i = 7; i >= 0 && sfn[i] == ' '; i--)
		;
	for (int j = 0; j <= i; j++) {
		char c = (char)sfn[j];
		out[len++] = (nt & 0x08) ? (char)tolower((unsigned char)c) : c;
	}

	/* Extension: strip trailing spaces */
	int ext_end;
	for (ext_end = 10; ext_end >= 8 && sfn[ext_end] == ' '; ext_end--)
		;
	if (ext_end >= 8) {
		out[len++] = '.';
		for (int j = 8; j <= ext_end; j++) {
			char c = (char)sfn[j];
			out[len++] = (nt & 0x10) ? (char)tolower((unsigned char)c) : c;
		}
	}
	out[len] = '\0';
}

/*
 * Check if a character is legal in a FAT 8.3 name position.
 */
static int
is_sfn_char(char c)
{
	if ((unsigned char)c < 0x20)
		return (0);
	switch (c) {
	case '"': case '*': case '+': case ',':
	case '.': case '/': case ':': case ';':
	case '<': case '=': case '>': case '?':
	case '[': case '\\': case ']': case '|':
		return (0);
	}
	return (1);
}

/*
 * Convert name to an 11-byte SFN (space-padded, uppercase, no dot).
 * Returns 1 if the name fits cleanly in 8.3; 0 if it needs LFN.
 * Either way, sfn_out is filled with a valid SFN (possibly with ~1 tail).
 */
static int
name_to_sfn(const char *name, u_int8_t *sfn_out)
{
	const char	*dot;
	int		 base_len, ext_len, needs_lfn = 0;
	int		 i;

	memset(sfn_out, ' ', 11);

	dot = NULL;
	for (const char *p = name; *p; p++) {
		if (*p == '.')
			dot = p;
	}

	if (dot != NULL) {
		base_len = (int)(dot - name);
		ext_len  = (int)strlen(dot + 1);
	} else {
		base_len = (int)strlen(name);
		ext_len  = 0;
	}

	if (base_len > 8 || ext_len > 3)
		needs_lfn = 1;

	/* Fill base (up to 8 chars) */
	int out_i = 0;
	for (i = 0; i < base_len && out_i < 8; i++) {
		char c = fat_upcase(name[i]);
		if (!is_sfn_char(c)) {
			needs_lfn = 1;
			c = '_';
		}
		if (c == ' ')
			needs_lfn = 1;
		sfn_out[out_i++] = (u_int8_t)c;
	}

	/* Fill extension (up to 3 chars) */
	if (dot != NULL) {
		out_i = 8;
		for (i = 0; i < ext_len && out_i < 11; i++) {
			char c = fat_upcase(dot[1 + i]);
			if (!is_sfn_char(c)) {
				needs_lfn = 1;
				c = '_';
			}
			sfn_out[out_i++] = (u_int8_t)c;
		}
	}

	/* Add numeric tail if LFN is needed (always ~1 for now) */
	if (needs_lfn) {
		sfn_out[6] = '~';
		sfn_out[7] = '1';
	}

	return (!needs_lfn);
}

/* ------------------------------------------------------------------ */
/* LFN helpers                                                         */
/* ------------------------------------------------------------------ */

#define LFN_CHARS_PER_ENTRY	13

/*
 * Extract the 13 UTF-16LE characters from an LFN entry into buf[].
 * Non-ASCII and 0xFFFF padding become '?' and '\0' respectively.
 */
static void
lfn_extract_chars(const u_int8_t *raw, char *buf)
{
	static const int offsets[13] = {
		1, 3, 5, 7, 9,
		14, 16, 18, 20, 22, 24,
		28, 30
	};
	int	 i;

	for (i = 0; i < 13; i++) {
		u_int16_t uc = le16(raw + offsets[i]);
		if (uc == 0xFFFF || uc == 0x0000)
			buf[i] = '\0';
		else if (uc > 0x7F)
			buf[i] = '?';
		else
			buf[i] = (char)(uc & 0x7F);
	}
}

/*
 * Write 13 ASCII chars as UTF-16LE into an LFN entry buffer.
 * Remaining slots (after NUL or end) are filled with 0xFFFF.
 */
static void
lfn_put_chars(u_int8_t *raw, const char *chars)
{
	static const int offsets[13] = {
		1, 3, 5, 7, 9,
		14, 16, 18, 20, 22, 24,
		28, 30
	};
	int	 i;
	int	 done = 0;

	for (i = 0; i < 13; i++) {
		u_int16_t uc;
		if (done) {
			uc = 0xFFFF;
		} else if (chars[i] == '\0') {
			uc   = 0x0000;
			done = 1;
		} else {
			uc = (u_int16_t)(unsigned char)chars[i];
		}
		raw[offsets[i]]     = (u_int8_t)(uc);
		raw[offsets[i] + 1] = (u_int8_t)(uc >> 8);
	}
}

/* ------------------------------------------------------------------ */
/* Iterator internals                                                  */
/* ------------------------------------------------------------------ */

static u_int32_t
iter_sector(struct fat_fs *fs, struct fat_dir_iter *it)
{
	if (it->cluster == 0)
		return (fs->root_lba + it->sector_in_cluster);
	return (fat_cluster_to_lba(fs, it->cluster) + it->sector_in_cluster);
}

/* Advance to next 32-byte slot.  Returns -1 at end of directory. */
static int
iter_advance(struct fat_fs *fs, struct fat_dir_iter *it)
{
	it->entry_offset += 32;
	if (it->entry_offset < 512)
		return (0);

	it->entry_offset = 0;
	it->sector_in_cluster++;

	if (it->cluster == 0) {
		/* FAT12/16 fixed root region */
		if (it->sector_in_cluster >= fs->root_sectors)
			return (-1);
	} else {
		if (it->sector_in_cluster >= fs->sectors_per_cluster) {
			u_int32_t next = fat_cluster_next(fs, it->cluster);
			if (next == FAT_CLUSTER_EOC || next < 2)
				return (-1);
			it->cluster		= next;
			it->sector_in_cluster	= 0;
		}
	}
	return (0);
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void
fat_dir_iter_open(struct fat_fs *fs, u_int32_t cluster, struct fat_dir_iter *it)
{
	it->fs			= fs;
	it->cluster		= cluster;
	it->sector_in_cluster	= 0;
	it->entry_offset	= 0;
	it->lfn[0]		= '\0';
	it->lfn_seq		= 0;
	it->lfn_checksum	= 0;
}

int
fat_dir_iter_next(struct fat_dir_iter *it, char *name_out,
    struct fat_raw_dirent *entry_out,
    u_int32_t *entry_sector, u_int16_t *entry_offset)
{
	struct fat_fs	*fs = it->fs;
	u_int8_t		 buf[512];
	u_int8_t		*raw;

	for (;;) {
		u_int32_t sec = iter_sector(fs, it);
		if (fat_sector_read(fs, sec, buf) != 0)
			return (-1);

		raw = buf + it->entry_offset;

		if (raw[0] == 0x00)	/* end of directory */
			return (-1);

		if (raw[0] == 0xE5) {	/* deleted — skip, clear LFN state */
			it->lfn[0]   = '\0';
			it->lfn_seq  = 0;
			if (iter_advance(fs, it) != 0)
				return (-1);
			continue;
		}

		if (raw[11] == FAT_ATTR_LFN &&
		    (raw[26] == 0 && raw[27] == 0)) {
			/* LFN entry */
			u_int8_t	 seq      = raw[0];
			u_int8_t	 seq_num  = seq & 0x1F;
			char	 chars[13];

			if (seq & 0x40) {
				/* First LFN entry encountered (last in name) */
				it->lfn_seq	 = seq_num;
				it->lfn_checksum = raw[13];
				memset(it->lfn, 0, sizeof(it->lfn));
			}

			lfn_extract_chars(raw, chars);

			/* Place chars at the correct offset in the LFN buffer */
			int	 pos   = (seq_num - 1) * LFN_CHARS_PER_ENTRY;
			int	 i;
			for (i = 0; i < 13; i++) {
				if (pos + i < 255)
					it->lfn[pos + i] = chars[i];
			}

			if (iter_advance(fs, it) != 0)
				return (-1);
			continue;
		}

		/* Volume label — skip */
		if (raw[11] & FAT_ATTR_VOL && !(raw[11] & FAT_ATTR_DIR)) {
			it->lfn[0]  = '\0';
			it->lfn_seq = 0;
			if (iter_advance(fs, it) != 0)
				return (-1);
			continue;
		}

		/* Regular or directory entry */
		if (entry_sector)	*entry_sector = sec;
		if (entry_offset)	*entry_offset = it->entry_offset;
		memcpy(entry_out, raw, sizeof(struct fat_raw_dirent));

		/* Use LFN if we accumulated one with a matching checksum */
		if (it->lfn[0] != '\0' &&
		    sfn_checksum(raw) == it->lfn_checksum) {
			strncpy(name_out, it->lfn, 255);
			name_out[255] = '\0';
		} else {
			sfn_to_name(raw, name_out);
		}
		it->lfn[0]  = '\0';
		it->lfn_seq = 0;

		if (iter_advance(fs, it) != 0) {
			/* Ignore advance failure — we already have the entry */
		}
		return (0);
	}
}

int
fat_dir_find(struct fat_fs *fs, u_int32_t dir_cluster, const char *name,
    struct fat_raw_dirent *out,
    u_int32_t *entry_sector, u_int16_t *entry_offset)
{
	struct fat_dir_iter	 it;
	char			 ename[256];
	struct fat_raw_dirent	 ent;
	u_int32_t		 sec;
	u_int16_t		 off;

	fat_dir_iter_open(fs, dir_cluster, &it);
	while (fat_dir_iter_next(&it, ename, &ent, &sec, &off) == 0) {
		if (fat_names_equal(ename, name)) {
			if (out)		memcpy(out, &ent, sizeof(ent));
			if (entry_sector)	*entry_sector = sec;
			if (entry_offset)	*entry_offset = off;
			return (0);
		}
	}
	return (-1);
}

int
fat_dir_update_size(struct fat_fs *fs, u_int32_t entry_sector,
    u_int16_t entry_offset, u_int32_t new_size)
{
	u_int8_t	 buf[512];

	if (fat_sector_read(fs, entry_sector, buf) != 0)
		return (-1);
	put_le32(buf + entry_offset + 28, new_size);
	return (fat_sector_write(fs, entry_sector, buf));
}

int
fat_dir_delete_entry(struct fat_fs *fs, u_int32_t entry_sector,
    u_int16_t entry_offset)
{
	u_int8_t	 buf[512];

	if (fat_sector_read(fs, entry_sector, buf) != 0)
		return (-1);
	buf[entry_offset] = 0xE5;
	return (fat_sector_write(fs, entry_sector, buf));
}

/*
 * Delete an 8.3 entry AND any preceding LFN entries that belong to it.
 * Scans dir_cluster from the start, tracking up to 20 LFN positions, and
 * marks them all 0xE5 once the 8.3 entry at (entry_sec, entry_off) is found.
 */
int
fat_dir_delete_with_lfn(struct fat_fs *fs, u_int32_t dir_cluster,
    u_int32_t entry_sec, u_int16_t entry_off)
{
#define MAX_LFN_ENTRIES 20
	struct fat_dir_iter	 it;
	u_int8_t			 buf[512];

	struct { u_int32_t sec; u_int16_t off; } lfn_pos[MAX_LFN_ENTRIES];
	int	 lfn_count = 0;

	fat_dir_iter_open(fs, dir_cluster, &it);

	for (;;) {
		u_int32_t sec = iter_sector(fs, &it);
		u_int16_t off = it.entry_offset;

		if (fat_sector_read(fs, sec, buf) != 0)
			return (-1);

		u_int8_t first = buf[off];

		if (first == 0x00)
			break;

		if (first == 0xE5) {
			lfn_count = 0;
			if (iter_advance(fs, &it) != 0)
				break;
			continue;
		}

		u_int8_t attr = buf[off + 11];

		if (attr == FAT_ATTR_LFN && (buf[off + 26] == 0 && buf[off + 27] == 0)) {
			if (buf[off] & 0x40)
				lfn_count = 0;
			if (lfn_count < MAX_LFN_ENTRIES) {
				lfn_pos[lfn_count].sec = sec;
				lfn_pos[lfn_count].off = off;
				lfn_count++;
			}
			if (iter_advance(fs, &it) != 0)
				break;
			continue;
		}

		/* Regular entry — check if this is our target */
		if (sec == entry_sec && off == entry_off) {
			/* Mark LFN entries deleted */
			for (int i = 0; i < lfn_count; i++) {
				if (fat_sector_read(fs, lfn_pos[i].sec, buf) != 0)
					continue;
				buf[lfn_pos[i].off] = 0xE5;
				fat_sector_write(fs, lfn_pos[i].sec, buf);
			}
			/* Mark the 8.3 entry deleted */
			if (fat_sector_read(fs, sec, buf) != 0)
				return (-1);
			buf[off] = 0xE5;
			return (fat_sector_write(fs, sec, buf));
		}

		lfn_count = 0;
		if (iter_advance(fs, &it) != 0)
			break;
	}
	return (-1);
#undef MAX_LFN_ENTRIES
}

/*
 * Scan the directory at dir_cluster for n_slots consecutive free (0x00 or
 * 0xE5) 32-byte entries.  Returns the sector/offset of the FIRST free slot,
 * or -1 if not enough space.
 */
static int
find_free_slots(struct fat_fs *fs, u_int32_t dir_cluster, int n_slots,
    u_int32_t *first_sec, u_int16_t *first_off)
{
	struct fat_dir_iter	 it;
	u_int8_t			 buf[512];
	int			 run = 0;
	u_int32_t		 run_sec = 0;
	u_int16_t		 run_off = 0;

	fat_dir_iter_open(fs, dir_cluster, &it);

	for (;;) {
		u_int32_t sec = iter_sector(fs, &it);
		if (fat_sector_read(fs, sec, buf) != 0)
			return (-1);

		u_int8_t first = buf[it.entry_offset];

		if (first == 0x00 || first == 0xE5) {
			if (run == 0) {
				run_sec = sec;
				run_off = it.entry_offset;
			}
			run++;
			if (run == n_slots) {
				*first_sec = run_sec;
				*first_off = run_off;
				return (0);
			}
		} else {
			run = 0;
		}

		if (first == 0x00)
			break;	/* no more entries after this */

		if (iter_advance(fs, &it) != 0)
			break;
	}
	return (-1);
}

int
fat_dir_create_entry(struct fat_fs *fs, u_int32_t dir_cluster,
    const char *name, u_int8_t attr, u_int32_t start_cluster,
    u_int32_t *entry_sector, u_int16_t *entry_offset)
{
	u_int8_t		 sfn[11];
	int		 is_short;
	int		 name_len, n_lfn, n_slots;
	u_int32_t	 first_sec;
	u_int16_t	 first_off;
	u_int8_t		 buf[512];
	u_int8_t		 checksum;

	is_short = name_to_sfn(name, sfn);
	name_len = (int)strlen(name);
	n_lfn    = is_short ? 0 : (name_len + LFN_CHARS_PER_ENTRY - 1) /
	    LFN_CHARS_PER_ENTRY;
	n_slots  = n_lfn + 1;

	if (find_free_slots(fs, dir_cluster, n_slots,
	    &first_sec, &first_off) != 0) {
		/* Directory full — try to grow it (FAT32 and subdirs only). */
		if (dir_cluster < 2 && fs->type != 32) {
			/* FAT12/16 fixed root cannot grow */
			kprintf("fat_dir_create_entry: root dir full\n");
			return (-1);
		}
		/* Walk to the last cluster of this directory chain. */
		u_int32_t last = dir_cluster;
		for (;;) {
			u_int32_t next = fat_cluster_next(fs, last);
			if (next == FAT_CLUSTER_EOC || next < 2)
				break;
			last = next;
		}
		u_int32_t new_clust = fat_cluster_alloc(fs, last);
		if (new_clust < 2) {
			kprintf("fat_dir_create_entry: disk full\n");
			return (-1);
		}
		/* Zero the new cluster so entries read as 0x00 (end marker). */
		u_int32_t clba = fat_cluster_to_lba(fs, new_clust);
		memset(buf, 0, sizeof(buf));
		for (u_int32_t s = 0; s < fs->sectors_per_cluster; s++)
			fat_sector_write(fs, clba + s, buf);
		/* Retry slot search — should now succeed. */
		if (find_free_slots(fs, dir_cluster, n_slots,
		    &first_sec, &first_off) != 0) {
			kprintf("fat_dir_create_entry: no free slots after grow\n");
			return (-1);
		}
	}

	checksum = sfn_checksum(sfn);

	/* Write LFN entries (in reverse order before the SFN) */
	if (n_lfn > 0) {
		struct fat_dir_iter	 it;
		u_int32_t		 sec = first_sec;
		u_int16_t		 off = first_off;

		fat_dir_iter_open(fs, dir_cluster, &it);

		/*
		 * Walk the cluster chain to find the cluster that actually
		 * contains first_sec.  A simple subtraction from dir_cluster's
		 * LBA is wrong for multi-cluster directories where the free run
		 * may start in a later cluster.
		 */
		u_int32_t it_cluster = (dir_cluster == 0) ? 0 : dir_cluster;
		if (dir_cluster != 0) {
			u_int32_t wc = dir_cluster;
			while (wc >= 2 && wc < FAT_CLUSTER_BAD) {
				u_int32_t clba = fat_cluster_to_lba(fs, wc);
				if (sec >= clba &&
				    sec < clba + fs->sectors_per_cluster) {
					it_cluster = wc;
					break;
				}
				wc = fat_cluster_next(fs, wc);
			}
		}
		it.cluster		= it_cluster;
		it.sector_in_cluster	= (dir_cluster == 0)
		    ? (sec - fs->root_lba)
		    : (sec - fat_cluster_to_lba(fs, it_cluster));
		it.entry_offset		= off;

		for (int seq = n_lfn; seq >= 1; seq--) {
			u_int8_t	 entry[32];
			char	 chars[13];
			int	 char_start = (seq - 1) * LFN_CHARS_PER_ENTRY;
			int	 i;

			memset(entry, 0, 32);
			for (i = 0; i < 13; i++) {
				int idx = char_start + i;
				chars[i] = (idx < name_len) ? name[idx] : '\0';
			}

			entry[0]  = (u_int8_t)(seq |
			    (seq == n_lfn ? 0x40 : 0));
			entry[11] = FAT_ATTR_LFN;
			entry[12] = 0;
			entry[13] = checksum;
			entry[26] = 0;
			entry[27] = 0;
			lfn_put_chars(entry, chars);

			sec = iter_sector(fs, &it);
			if (fat_sector_read(fs, sec, buf) != 0)
				return (-1);
			memcpy(buf + it.entry_offset, entry, 32);
			if (fat_sector_write(fs, sec, buf) != 0)
				return (-1);

			iter_advance(fs, &it);
		}

		/* Now it points at the SFN slot */
		sec = iter_sector(fs, &it);
		if (fat_sector_read(fs, sec, buf) != 0)
			return (-1);

		memset(buf + it.entry_offset, 0, 32);
		memcpy(buf + it.entry_offset, sfn, 11);
		buf[it.entry_offset + 11] = attr;
		buf[it.entry_offset + 26] = (u_int8_t)(start_cluster & 0xFF);
		buf[it.entry_offset + 27] = (u_int8_t)(start_cluster >> 8);
		buf[it.entry_offset + 20] = (u_int8_t)(start_cluster >> 16);
		buf[it.entry_offset + 21] = (u_int8_t)(start_cluster >> 24);
		/* file_size stays 0 at creation */

		if (fat_sector_write(fs, sec, buf) != 0)
			return (-1);

		if (entry_sector)	*entry_sector = sec;
		if (entry_offset)	*entry_offset = it.entry_offset;
		return (0);
	}

	/* SFN-only path */
	if (fat_sector_read(fs, first_sec, buf) != 0)
		return (-1);

	memset(buf + first_off, 0, 32);
	memcpy(buf + first_off, sfn, 11);
	buf[first_off + 11] = attr;
	buf[first_off + 26] = (u_int8_t)(start_cluster & 0xFF);
	buf[first_off + 27] = (u_int8_t)(start_cluster >> 8);
	buf[first_off + 20] = (u_int8_t)(start_cluster >> 16);
	buf[first_off + 21] = (u_int8_t)(start_cluster >> 24);

	if (fat_sector_write(fs, first_sec, buf) != 0)
		return (-1);

	if (entry_sector)	*entry_sector = first_sec;
	if (entry_offset)	*entry_offset = first_off;
	return (0);
}

/* ------------------------------------------------------------------ */
/* Path resolution                                                     */
/* ------------------------------------------------------------------ */

int
fat_path_resolve(struct fat_fs *fs, const char *path,
    u_int32_t *dir_cluster_out,
    struct fat_raw_dirent *file_entry_out,
    u_int32_t *entry_sector, u_int16_t *entry_offset)
{
	char		 comp[256];
	u_int32_t	 cur_cluster;
	struct fat_raw_dirent	 ent;
	u_int32_t	 sec;
	u_int16_t	 off;
	const char	*p = path;

	/* Start at root */
	cur_cluster = (fs->type == 32) ? fs->root_cluster : 0;

	/* Strip leading slash */
	if (*p == '/')
		p++;

	while (*p) {
		/* Extract next component */
		int	 i = 0;
		while (*p && *p != '/' && i < 255)
			comp[i++] = *p++;
		comp[i] = '\0';
		/* Consume any remaining characters in an over-long component */
		if (i == 255 && *p && *p != '/') {
			while (*p && *p != '/')
				p++;
			return (-1); /* ENAMETOOLONG */
		}
		if (*p == '/')
			p++;

		if (comp[0] == '\0' || (comp[0] == '.' && comp[1] == '\0')) {
			/* '.' — stay in current directory */
			continue;
		}

		if (comp[0] == '.' && comp[1] == '.' && comp[2] == '\0') {
			/*
			 * '..' — walk up.  The '..' entry's cluster fields
			 * point to the parent directory.
			 */
			if (fat_dir_find(fs, cur_cluster, "..", &ent,
			    &sec, &off) == 0) {
				u_int32_t parent = ((u_int32_t)le16(
				    (u_int8_t *)&ent.clus_hi) << 16) |
				    le16((u_int8_t *)&ent.clus_lo);
				cur_cluster = (parent == 0 && fs->type != 32)
				    ? 0 : parent;
			}
			continue;
		}

		/* Is there more path after this component? */
		int	 more = (*p != '\0');

		if (fat_dir_find(fs, cur_cluster, comp, &ent, &sec, &off) != 0) {
			/* Not found */
			if (dir_cluster_out)	*dir_cluster_out = cur_cluster;
			return (-1);
		}

		if (more) {
			/* Must be a directory to descend into */
			if (!(ent.attr & FAT_ATTR_DIR))
				return (-1);
			cur_cluster = ((u_int32_t)ent.clus_hi << 16) |
			    ent.clus_lo;
			if (cur_cluster == 0 && fs->type != 32)
				cur_cluster = 0;
		} else {
			/* Final component found */
			if (dir_cluster_out)	*dir_cluster_out = cur_cluster;
			if (file_entry_out)	memcpy(file_entry_out, &ent,
						    sizeof(ent));
			if (entry_sector)	*entry_sector = sec;
			if (entry_offset)	*entry_offset = off;
			return (0);
		}
	}

	/* Path ended on a directory separator — the path IS a directory */
	if (dir_cluster_out)	*dir_cluster_out = cur_cluster;
	return (-1);
}

int
fat_path_to_dir_cluster(struct fat_fs *fs, const char *path,
    u_int32_t *cluster_out)
{
	struct fat_raw_dirent	 ent;
	u_int32_t		 dir_cluster, sec;
	u_int16_t		 off;
	int			 ret;

	/* Root directory */
	if (path[0] == '\0' || (path[0] == '/' && path[1] == '\0')) {
		*cluster_out = (fs->type == 32) ? fs->root_cluster : 0;
		return (0);
	}

	ret = fat_path_resolve(fs, path, &dir_cluster, &ent, &sec, &off);
	if (ret == 0 && (ent.attr & FAT_ATTR_DIR)) {
		*cluster_out = ((u_int32_t)ent.clus_hi << 16) | ent.clus_lo;
		if (*cluster_out == 0 && fs->type != 32)
			*cluster_out = 0;
		return (0);
	}

	/* fat_path_resolve returned -1 but may have left dir_cluster valid */
	if (ret != 0) {
		*cluster_out = (fs->type == 32) ? fs->root_cluster : 0;
		return (-1);
	}
	return (-1);
}

/* ------------------------------------------------------------------ */
/* mkdir / rmdir / unlink                                              */
/* ------------------------------------------------------------------ */

int
fat_dir_mkdir(struct fat_fs *fs, const char *path)
{
	char		 parent_path[256];
	const char	*base;
	u_int32_t	 parent_cluster, new_cluster;
	u_int32_t	 entry_sec, dot_sec;
	u_int16_t	 entry_off, dot_off;
	u_int8_t		 buf[512];

	/* Split path into parent and basename */
	strncpy(parent_path, path, 255);
	parent_path[255] = '\0';
	base = path;
	for (const char *p = path; *p; p++)
		if (*p == '/')
			base = p + 1;

	if (base != path) {
		int	 plen = (int)(base - path - 1);
		if (plen >= 256)
			plen = 255;
		memcpy(parent_path, path, (size_t)plen);
		parent_path[plen] = '\0';
		if (fat_path_to_dir_cluster(fs, parent_path,
		    &parent_cluster) != 0)
			return (-1);
	} else {
		parent_cluster = (fs->type == 32) ? fs->root_cluster : 0;
	}

	/* Allocate one cluster for the new directory */
	new_cluster = fat_cluster_alloc(fs, 0);
	if (new_cluster == 0)
		return (-1);

	/* Zero out the cluster */
	memset(buf, 0, 512);
	u_int32_t lba = fat_cluster_to_lba(fs, new_cluster);
	for (u_int32_t i = 0; i < fs->sectors_per_cluster; i++) {
		if (fat_sector_write(fs, lba + i, buf) != 0) {
			fat_cluster_free_chain(fs, new_cluster);
			return (-1);
		}
	}

	/* Write '.' entry */
	if (fat_sector_read(fs, lba, buf) != 0)
		return (-1);

	u_int8_t dot_sfn[11];
	memset(dot_sfn, ' ', 11);
	dot_sfn[0] = '.';
	memset(buf, 0, 32);
	memcpy(buf, dot_sfn, 11);
	buf[11] = FAT_ATTR_DIR;
	buf[26] = (u_int8_t)(new_cluster & 0xFF);
	buf[27] = (u_int8_t)(new_cluster >> 8);
	buf[20] = (u_int8_t)(new_cluster >> 16);
	buf[21] = (u_int8_t)(new_cluster >> 24);
	dot_sec = lba;
	dot_off = 0;

	/* Write '..' entry */
	u_int8_t dotdot_sfn[11];
	memset(dotdot_sfn, ' ', 11);
	dotdot_sfn[0] = '.';
	dotdot_sfn[1] = '.';
	memset(buf + 32, 0, 32);
	memcpy(buf + 32, dotdot_sfn, 11);
	buf[32 + 11] = FAT_ATTR_DIR;
	buf[32 + 26] = (u_int8_t)(parent_cluster & 0xFF);
	buf[32 + 27] = (u_int8_t)(parent_cluster >> 8);
	buf[32 + 20] = (u_int8_t)(parent_cluster >> 16);
	buf[32 + 21] = (u_int8_t)(parent_cluster >> 24);

	if (fat_sector_write(fs, lba, buf) != 0) {
		fat_cluster_free_chain(fs, new_cluster);
		return (-1);
	}
	(void)dot_sec;
	(void)dot_off;

	/* Create entry in parent directory */
	if (fat_dir_create_entry(fs, parent_cluster, base,
	    FAT_ATTR_DIR, new_cluster, &entry_sec, &entry_off) != 0) {
		fat_cluster_free_chain(fs, new_cluster);
		return (-1);
	}
	return (0);
}

int
fat_dir_rmdir(struct fat_fs *fs, const char *path)
{
	struct fat_raw_dirent	 ent;
	u_int32_t		 dir_cluster, target_cluster, sec;
	u_int16_t		 off;
	struct fat_dir_iter	 it;
	char			 name[256];
	struct fat_raw_dirent	 child;
	u_int32_t		 csec;
	u_int16_t		 coff;

	if (fat_path_resolve(fs, path, &dir_cluster, &ent, &sec, &off) != 0)
		return (-1);
	if (!(ent.attr & FAT_ATTR_DIR))
		return (-1);

	target_cluster = ((u_int32_t)ent.clus_hi << 16) | ent.clus_lo;

	/* Verify directory is empty (only '.' and '..' allowed) */
	fat_dir_iter_open(fs, target_cluster, &it);
	while (fat_dir_iter_next(&it, name, &child, &csec, &coff) == 0) {
		if (name[0] == '.' &&
		    (name[1] == '\0' || (name[1] == '.' && name[2] == '\0')))
			continue;
		return (-1);	/* not empty */
	}

	if (fat_dir_delete_with_lfn(fs, dir_cluster, sec, off) != 0)
		return (-1);
	return (fat_cluster_free_chain(fs, target_cluster));
}

int
fat_dir_unlink(struct fat_fs *fs, const char *path)
{
	struct fat_raw_dirent	 ent;
	u_int32_t		 dir_cluster, start_cluster, sec;
	u_int16_t		 off;

	if (fat_path_resolve(fs, path, &dir_cluster, &ent, &sec, &off) != 0)
		return (-1);
	if (ent.attr & FAT_ATTR_DIR)
		return (-1);

	start_cluster = ((u_int32_t)ent.clus_hi << 16) | ent.clus_lo;

	if (fat_dir_delete_with_lfn(fs, dir_cluster, sec, off) != 0)
		return (-1);

	if (start_cluster >= 2)
		fat_cluster_free_chain(fs, start_cluster);

	return (0);
}
