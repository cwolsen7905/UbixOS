/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * Minimal authentication daemon for the aarch64 bring-up: provides the "authd"
 * MPI mailbox the real /bin/login talks to, so the genuine login -> shell chain
 * works before libpw/BearSSL are ported to aarch64.  Plaintext check against the
 * built-in default (root / user — same as the i386 image); the real authd's
 * PBKDF2-over-BearSSL verification replaces this once the crypto libs build.
 *
 * Built static against musl; uses the native MPI syscalls by raw SVC.
 */
#include <unistd.h>
#include <string.h>

#define NATIVE 0x8000
#define MPI_CREATE (NATIVE | 50)
#define MPI_POST (NATIVE | 52)
#define MPI_FETCH (NATIVE | 53)
#define SYS_SCHED_YIELD 331

#define AUTHD_MBOX "authd"
#define AUTHD_MSG_REQUEST 1
#define AUTHD_MSG_RESPONSE 2
#define MPI_ASYNC 1

/* Must match the kernel's mpi_message (data first, 248 bytes). */
struct mpi_msg
{
	char data[248];
	unsigned int header;
	int pid;
	void *next;
};

/* Must match include/authd.h. */
struct auth_request
{
	char reply_mbox[32];
	char username[32];
	char password[32];
};
struct auth_response
{
	int ok, uid, gid;
	char shell[80];
	char home[128];
};

static long nsys(long n, long a, long b, long c)
{
	register long x8 __asm__("x8") = n;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	__asm__ volatile("svc #0" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2) : "memory");
	return x0;
}

int main(void)
{
	struct mpi_msg m;

	if (nsys(MPI_CREATE, (long)AUTHD_MBOX, 0, 0) != 0)
	{
		write(2, "authd: createMbox failed\n", 25);
		return 1;
	}
	write(1, "authd: ready\n", 13);

	for (;;)
	{
		if (nsys(MPI_FETCH, (long)AUTHD_MBOX, (long)&m, 0) != 0)
		{
			nsys(SYS_SCHED_YIELD, 0, 0, 0); /* nothing pending — yield */
			continue;
		}
		if (m.header != AUTHD_MSG_REQUEST)
			continue;

		struct auth_request req;
		struct auth_response resp;
		struct mpi_msg out;
		memcpy(&req, m.data, sizeof(req));
		memset(&resp, 0, sizeof(resp));

		if (strcmp(req.username, "root") == 0 && strcmp(req.password, "user") == 0)
		{
			resp.ok = 1;
			resp.uid = 0;
			resp.gid = 0;
			strcpy(resp.shell, "/bin/shell");
			strcpy(resp.home, "/");
		}

		memset(&out, 0, sizeof(out));
		out.header = AUTHD_MSG_RESPONSE;
		memcpy(out.data, &resp, sizeof(resp));
		nsys(MPI_POST, (long)req.reply_mbox, MPI_ASYNC, (long)&out);
	}
	return 0;
}
