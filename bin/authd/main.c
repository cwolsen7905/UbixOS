/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
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
 * authd — UbixOS authentication daemon.
 *
 * Owns /etc/userdb exclusively.  All other processes (login, guilogin,
 * future sudo/ssh) send an AUTH_REQUEST MPI message and receive an
 * AUTH_RESPONSE.  authd is the only process that ever reads the credential
 * store.
 *
 * Protocol: see include/authd.h
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/mpi.h>
#include <authd.h>

#define USERDB_PATH  "/etc/userdb"
#define USERDB_MAX   64

/* Must match the binary layout written by tools/userdb */
struct userdb_entry {
	char username[32];
	char password[32];
	int  uid;
	int  gid;
	char shell[128];
	char realname[256];
	char path[256];       /* home directory */
};

static struct userdb_entry users[USERDB_MAX];
static int                 nusers = 0;

static int
load_userdb(void)
{
	FILE   *f;
	size_t  n;

	f = fopen(USERDB_PATH, "r");
	if (f == NULL)
		return (-1);
	n = fread(users, sizeof(struct userdb_entry), USERDB_MAX, f);
	fclose(f);
	nusers = (int)n;
	return (nusers);
}

int
main(void)
{
	mpi_message_t        msg;
	mpi_message_t        rmsg;
	struct auth_request  *req;
	struct auth_response  resp;
	int                   i;

	if (mpi_createMbox(AUTHD_MBOX) != 0) {
		printf("authd: failed to create mailbox\n");
		exit(1);
	}

	if (load_userdb() < 0) {
		printf("authd: cannot open %s — no authentication possible\n",
		    USERDB_PATH);
		/* keep running so clients get a clean rejection rather than hanging */
	}

	printf("authd: ready (%d user%s)\n", nusers, nusers == 1 ? "" : "s");
	fflush(stdout);

	for (;;) {
		if (mpi_fetchMessage(AUTHD_MBOX, &msg) != 0) {
			sched_yield();
			continue;
		}

		if (msg.header != AUTHD_MSG_REQUEST) {
			sched_yield();
			continue;
		}

		req = (struct auth_request *)msg.data;

		memset(&resp, 0, sizeof(resp));

		for (i = 0; i < nusers; i++) {
			if (strcmp(req->username, users[i].username) == 0 &&
			    strcmp(req->password, users[i].password) == 0) {
				resp.ok  = 1;
				resp.uid = users[i].uid;
				resp.gid = users[i].gid;
				strncpy(resp.shell, users[i].shell,
				    AUTH_SHELL_MAX - 1);
				strncpy(resp.home, users[i].path,
				    AUTH_HOME_MAX - 1);
				break;
			}
		}

		if (req->reply_mbox[0] == '\0')
			continue;

		memset(&rmsg, 0, sizeof(rmsg));
		rmsg.header = AUTHD_MSG_RESPONSE;
		memcpy(rmsg.data, &resp, sizeof(resp));
		mpi_postMessage(req->reply_mbox, AUTHD_MSG_RESPONSE, &rmsg);
	}

	return (0);
}
