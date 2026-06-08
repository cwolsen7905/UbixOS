/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * MPI self-test (aarch64 bring-up): exercise the UbixOS native message-passing
 * syscalls (createMbox/postMessage/fetchMessage) from EL0 to prove the native
 * (int $0x81-equivalent) MPI path works — the gateway the real init/authd/login
 * chain rides on.  Issues the native syscalls by raw SVC (x8 = num | 0x8000),
 * matching lib/ubix_api's stubs, so it needs no library.
 */
#include <unistd.h>
#include <string.h>

#define NATIVE 0x8000
#define MPI_CREATE (NATIVE | 50)
#define MPI_POST (NATIVE | 52)
#define MPI_FETCH (NATIVE | 53)

/* Must match the kernel's mpi_message (data first, 248 bytes). */
struct mpi_msg
{
	char data[248];
	unsigned int header;
	int pid;
	void *next;
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
	struct mpi_msg m, r;

	if (nsys(MPI_CREATE, (long)"mt", 0, 0) != 0)
	{
		write(1, "mpi: createMbox FAILED\n", 23);
		return 1;
	}

	memset(&m, 0, sizeof(m));
	strcpy(m.data, "MPI round-trip OK");
	nsys(MPI_POST, (long)"mt", 1 /* async */, (long)&m);

	memset(&r, 0, sizeof(r));
	if (nsys(MPI_FETCH, (long)"mt", (long)&r, 0) == 0)
	{
		write(1, "mpi: fetched: ", 14);
		write(1, r.data, strlen(r.data));
		write(1, "\n", 1);
	}
	else
	{
		write(1, "mpi: fetchMessage FAILED\n", 25);
	}
	return 0;
}
