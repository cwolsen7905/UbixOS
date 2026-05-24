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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static void usage(void);
static void show_mounts(void);

int
main(int argc, char *const argv[])
{
	int ch;

	/* No arguments: show currently mounted filesystems */
	if (argc == 1) {
		show_mounts();
		return 0;
	}

	while ((ch = getopt(argc, argv, "adlF:fo:prwt:uv")) != -1) {
		switch (ch) {
		default:
			usage();
		}
	}

	return 0;
}

static void
show_mounts(void)
{
	int  fd;
	char buf[2048];
	int  n;

	fd = open("/proc/mounts", O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "mount: cannot open /proc/mounts\n");
		return;
	}

	while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
		char *line, *ctx;
		buf[n] = '\0';
		/* Parse "dev mountpoint fstype perms 0 0" and reformat */
		for (line = strtok_r(buf, "\n", &ctx); line != NULL;
		     line = strtok_r(NULL, "\n", &ctx)) {
			char dev[256], mp[256], fstype[32], perms[32];
			if (sscanf(line, "%255s %255s %31s %31s",
			    dev, mp, fstype, perms) == 4) {
				printf("%s on %s type %s (%s)\n",
				    dev, mp, fstype, perms);
			}
		}
	}

	close(fd);
}

static void
usage(void)
{
	fprintf(stderr,
	    "usage: mount [-adflpruvw] [-F fstab] [-o options] "
	    "[-t ufs | external_type]\n"
	    "       mount [-dfpruvw] special | node\n"
	    "       mount [-dfpruvw] [-o options] "
	    "[-t ufs | external_type] special node\n");
	exit(1);
}
