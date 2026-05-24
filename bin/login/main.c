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
#include <sched.h>
#include <sys/wait.h>
#include <sys/mpi.h>
#include <api/ubix.h>
#include <authd.h>

#define MOTD_PATH   "/etc/motd"
#define REPLY_MBOX_MAX 32

static char reply_mbox[REPLY_MBOX_MAX];

static char  argv0_buf[64];
static char *argv_shell[2] = { argv0_buf, NULL };
static char  envp_buf[11][128];
static char *envp_shell[12];

static char *
pgets(char *string, int maxlen)
{
	int count = 0;
	unsigned char ch;

	tty_setraw(1);
	while (1) {
		if (read(0, &ch, 1) != 1)
			continue;
		if (ch == '\n' || ch == '\r') {
			printf("\n");
			fflush(stdout);
			break;
		} else if ((ch == 8 || ch == 127) && count > 0) {
			count--;
		} else if (ch == 0) {
			if (count == 0) { tty_setraw(0); return (NULL); }
		} else if (count < maxlen - 1) {
			string[count++] = (char)ch;
			printf("*");
			fflush(stdout);
		}
	}
	tty_setraw(0);
	string[count] = '\0';
	return (string);
}

static struct auth_response
do_auth(const char *username, const char *password)
{
	mpi_message_t        msg;
	mpi_message_t        rmsg;
	struct auth_request  req;
	struct auth_response resp;
	int                  waited;

	memset(&req,  0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	strncpy(req.reply_mbox, reply_mbox,   AUTH_MBOX_MAX - 1);
	strncpy(req.username,   username,     AUTH_USER_MAX - 1);
	strncpy(req.password,   password,     AUTH_PASS_MAX - 1);

	memset(&msg, 0, sizeof(msg));
	msg.header = AUTHD_MSG_REQUEST;
	memcpy(msg.data, &req, sizeof(req));

	/* Retry until authd's mailbox exists — it may still be starting up. */
	for (waited = 0; waited < 2000; waited++) {
		if (mpi_postMessage(AUTHD_MBOX, AUTHD_MSG_REQUEST, &msg) == 0)
			break;
		sched_yield();
		if (waited == 1999) {
			printf("login: authd not available\n");
			return (resp);
		}
	}

	/* Poll for response — authd replies promptly */
	for (waited = 0; waited < 1000; waited++) {
		if (mpi_fetchMessage(reply_mbox, &rmsg) == 0 &&
		    rmsg.header == AUTHD_MSG_RESPONSE) {
			memcpy(&resp, rmsg.data, sizeof(resp));
			return (resp);
		}
		sched_yield();
	}

	printf("login: timeout waiting for authd\n");
	return (resp);
}

int
main(int argc, char **argv, char **env)
{
	struct auth_response resp;
	char                 userName[AUTH_USER_MAX];
	char                 passWord[AUTH_PASS_MAX];
	char                 motd[384];
	FILE                *fd;
	int                  shellPid;
	int                  i;

	(void)argc; (void)argv; (void)env;

	if ((getuid() != 0) && (getgid() != 0)) {
		printf("This application must be run as root.\n");
		exit(-1);
	}

	snprintf(reply_mbox, sizeof(reply_mbox), "login.auth.%d", getpid());
	mpi_createMbox(reply_mbox);

login:
	printf("\nUbixOS/IA-32 (devel.ubixos.com) (console)");

getUsername:
	printf("\n\nLogin: ");
	fflush(stdout);
	fgets(userName, sizeof(userName), stdin);
	{ int n = strlen(userName); if (n > 0 && userName[n-1] == '\n') userName[n-1] = '\0'; }

	if (userName[0] == '\0')
		goto getUsername;

	printf("Password: ");
	fflush(stdout);
	if (pgets(passWord, sizeof(passWord)) == NULL)
		goto login;

	resp = do_auth(userName, passWord);

	if (!resp.ok) {
		printf("Login incorrect.\n");
		goto login;
	}

	shellPid = fork();
	if (shellPid == 0) {
		if (setuid(resp.uid) != 0)
			printf("login: setuid failed\n");
		if (setgid(resp.gid) != 0)
			printf("login: setgid failed\n");

		/* show motd */
		fd = fopen(MOTD_PATH, "r");
		if (fd != NULL) {
			size_t n = fread(motd, 1, sizeof(motd) - 1, fd);
			motd[n] = '\0';
			if (n > 0) printf("%s\n", motd);
			fclose(fd);
		}

		/* build envp from auth response */
		i = 0;
		snprintf(envp_buf[i], 128, "HOME=%s",  resp.home[0] ? resp.home : "/"); i++;
		snprintf(envp_buf[i], 128, "PWD=%s",   resp.home[0] ? resp.home : "/"); i++;
		snprintf(envp_buf[i], 128, "SHELL=%s", resp.shell[0] ? resp.shell : "/bin/shell"); i++;
		snprintf(envp_buf[i], 128, "PATH=/bin:/sbin:/usr/bin:/usr/sbin"); i++;
		snprintf(envp_buf[i], 128, "USER=%s",  userName); i++;
		snprintf(envp_buf[i], 128, "LOGNAME=%s", userName); i++;
		snprintf(envp_buf[i], 128, "HOST=Dev.uBixOS.com"); i++;
		snprintf(envp_buf[i], 128, "TERM=vt100"); i++;
		snprintf(envp_buf[i], 128, "LD_LIBRARY_PATH=/lib:/usr/lib"); i++;
		envp_buf[i][0] = '\0'; i++;
		for (int j = 0; j < i; j++) envp_shell[j] = envp_buf[j];
		envp_shell[i] = NULL;

		chdir(resp.home[0] ? resp.home : "/");
		fflush(stdout);

		/* argv[0] = "-basename" so shell sees itself as a login shell. */
		const char *shell_path = resp.shell[0] ? resp.shell : "/bin/shell";
		const char *base = strrchr(shell_path, '/');
		base = base ? base + 1 : shell_path;
		argv0_buf[0] = '-';
		strncpy(argv0_buf + 1, base, sizeof(argv0_buf) - 2);
		argv0_buf[sizeof(argv0_buf) - 1] = '\0';

		execve(shell_path, argv_shell, envp_shell);
		printf("login: failed to exec shell\n");
		exit(-1);
	}

	waitpid(shellPid, NULL, 0);

	goto login;
	return (0);
}
