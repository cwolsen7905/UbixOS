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

/*
 * automountd — UbixOS automount daemon.
 *
 * Phase 3+4: reads /etc/fstab and mounts all non-auto entries at startup
 * (devfs → /dev, procfs → /proc).
 *
 * Phase 6: listens on the "automountd" MPI mailbox for MPI_STORAGE_APPEARED
 * and MPI_STORAGE_DEPARTED events sent by USB/IDE drivers (Phase 5), and
 * mounts/unmounts removable devices at /mnt/<volname>.
 *
 * init starts this daemon (via etc/init.d/05-automountd) before any other
 * service so that /dev and /proc are live before login or ttyd run.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mpi.h>

/* mount/unmount syscall wrappers — FreeBSD ABI 21/22 */
int mount(const char *type, const char *path, int flags, void *data);
int unmount(const char *path, int flags);
#include <mpi/storage.h>
#include <sched.h>

#define FSTAB_PATH "/etc/fstab"
#define FSTAB_MAX 32
#define MNT_BASE "/mnt"

/* ── fstab entry ─────────────────────────────────────────────────────────── */

typedef struct
{
	char device[64];
	char mountpoint[256];
	char fstype[32];
	char options[64];
} fstab_entry_t;

/* ── fstab parser ────────────────────────────────────────────────────────── */

static int fstab_parse(const char *path, fstab_entry_t *out, int max)
{
	FILE *f;
	char line[512];
	int n = 0;

	f = fopen(path, "r");
	if (f == NULL)
	{
		fprintf(stderr, "automountd: cannot open %s\n", path);
		return (0);
	}

	while (n < max && fgets(line, sizeof(line), f) != NULL)
	{
		char *p = line;

		/* strip leading whitespace */
		while (*p == ' ' || *p == '\t')
			p++;

		/* skip comments and blank lines */
		if (*p == '#' || *p == '\0' || *p == '\n')
			continue;

		/* strip trailing newline */
		line[strcspn(line, "\r\n")] = '\0';

		if (sscanf(p, "%63s %255s %31s %63s", out[n].device, out[n].mountpoint, out[n].fstype, out[n].options) <
		    3)
			continue;

		n++;
	}

	fclose(f);
	return (n);
}

/* ── static mount (Phase 4) ──────────────────────────────────────────────── */

static void mount_entry(const fstab_entry_t *e)
{
	/* Pass the fstab device field as the mount data: a block-device FS (fat)
	 * names its partition there ("/dev/ad0s1"), which the kernel resolves to a
	 * (major, minor) via devfs.  Pseudo-FSes (devfs/procfs) use "none", which the
	 * kernel treats as deviceless. */
	const char *dev = (e->device[0] != '\0') ? e->device : NULL;

	mkdir(e->mountpoint, 0755);
	if (mount(e->fstype, e->mountpoint, 0, (void *)dev) != 0)
		fprintf(stderr, "automountd: mount %s (%s) at %s failed\n", e->fstype, e->device, e->mountpoint);
	else
		printf("automountd: mounted %s at %s\n", e->fstype, e->mountpoint);
}

/* ── dynamic mount table (Phase 6) ──────────────────────────────────────── */

#define MOUNT_TABLE_MAX 16

typedef struct
{
	char dev_path[STORAGE_DEV_MAX];
	char mountpath[STORAGE_MNT_MAX];
} mount_entry_t;

static mount_entry_t mount_table[MOUNT_TABLE_MAX];
static int mount_table_n = 0;

static void mount_table_add(const char *dev, const char *mnt)
{
	if (mount_table_n >= MOUNT_TABLE_MAX)
		return;
	strncpy(mount_table[mount_table_n].dev_path, dev, STORAGE_DEV_MAX - 1);
	strncpy(mount_table[mount_table_n].mountpath, mnt, STORAGE_MNT_MAX - 1);
	mount_table_n++;
}

static const char *mount_table_find(const char *dev)
{
	int i;
	for (i = 0; i < mount_table_n; i++)
		if (strcmp(mount_table[i].dev_path, dev) == 0)
			return (mount_table[i].mountpath);
	return (NULL);
}

static void mount_table_remove(const char *dev)
{
	int i;
	for (i = 0; i < mount_table_n; i++)
	{
		if (strcmp(mount_table[i].dev_path, dev) == 0)
		{
			mount_table[i] = mount_table[--mount_table_n];
			return;
		}
	}
}

/* ── dynamic mount/unmount (Phase 6) ─────────────────────────────────────── */

static void handle_storage_event(mpi_message_t *msg)
{
	mpi_storage_msg_t *s = (mpi_storage_msg_t *)msg->data;
	char mntpath[STORAGE_MNT_MAX];
	char volname[STORAGE_VOL_MAX + 1];
	int i;

	if (s->type == MPI_STORAGE_APPEARED)
	{
		/* volume_name is already lowercased by the kernel driver */
		strncpy(volname, s->volume_name, sizeof(volname) - 1);
		volname[sizeof(volname) - 1] = '\0';

		/* lowercase any remaining uppercase (defensive) */
		for (i = 0; volname[i]; i++)
			volname[i] = (char)tolower((unsigned char)volname[i]);

		if (volname[0] == '\0')
		{
			fprintf(stderr, "automountd: %s has no volume name\n", s->dev_path);
			return;
		}

		snprintf(mntpath, sizeof(mntpath), "%s/%s", MNT_BASE, volname);
		mkdir(MNT_BASE, 0755);
		mkdir(mntpath, 0755);

		if (mount(s->fstype, mntpath, 0, NULL) == 0)
		{
			mount_table_add(s->dev_path, mntpath);
			printf("automountd: mounted %s at %s\n", s->dev_path, mntpath);
		}
		else
		{
			fprintf(stderr, "automountd: mount %s at %s failed\n", s->dev_path, mntpath);
		}
	}
	else if (s->type == MPI_STORAGE_DEPARTED)
	{
		const char *mnt = mount_table_find(s->dev_path);
		if (mnt == NULL)
		{
			fprintf(stderr, "automountd: no mount recorded for %s\n", s->dev_path);
			return;
		}
		if (unmount(mnt, 0) == 0)
		{
			printf("automountd: unmounted %s\n", mnt);
			mount_table_remove(s->dev_path);
		}
		else
		{
			fprintf(stderr, "automountd: umount %s failed\n", mnt);
		}
	}
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(void)
{
	fstab_entry_t entries[FSTAB_MAX];
	mpi_message_t msg;
	int n, i;

	printf("automountd: starting\n");
	fflush(stdout);

	if (mpi_createMbox(AUTOMOUNTD_MBOX) != 0)
	{
		fprintf(stderr, "automountd: failed to create MPI mailbox\n");
		exit(1);
	}

	/* Phase 4: mount all static (non-auto) fstab entries */
	n = fstab_parse(FSTAB_PATH, entries, FSTAB_MAX);
	for (i = 0; i < n; i++)
	{
		if (strcmp(entries[i].fstype, "auto") == 0)
			continue;
		mount_entry(&entries[i]);
	}

	printf("automountd: static mounts done, waiting for device events\n");
	fflush(stdout);

	/* Phase 6: event loop — handle storage arrive/depart MPI messages */
	for (;;)
	{
		/* Block until a storage arrive/depart event arrives instead of
		 * busy-polling — automountd is idle until a device is (un)plugged. */
		if (mpi_waitMessage(AUTOMOUNTD_MBOX, &msg, 0) == 0)
			handle_storage_event(&msg);
	}

	return (0);
}
