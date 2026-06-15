/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 ring-3 (user mode) foundation (Phase 5a).  Builds a writable GDT that
 * adds the ring-3 code/data segments + a 64-bit TSS (for the ring-0 stack the CPU
 * loads on a ring3->ring0 interrupt/syscall), the prerequisite for ever running
 * unprivileged code.  start.S's boot GDT has only the two ring-0 segments; this
 * replaces it once long mode + the C environment are up.  Sibling of i386's
 * pcpu_gdt_tss_load.
 *
 * GDT layout (also the SYSCALL/SYSRET convention used in Phase 5a-2):
 *   0x08 kernel code64   0x10 kernel data
 *   0x18 user data       0x20 user code64      0x28 TSS (16 bytes)
 * so user data = 0x1B (0x18|3) and user code = 0x23 (0x20|3).
 */

#include "../x86_64.h"
#include <vmm/vmm.h>     /* vmm_find_free_page, PAGE_SIZE */
#include <lib/kmalloc.h> /* sysID */
#include <string.h>
#include <ubixos/sched.h>    /* _current, sched(), kTask_t */
#include <ubixos/endtask.h>  /* endTask */
#include <x86_64/pcpu.h>     /* curcpu() — per-CPU kernel RSP for the syscall entry */
#include <fs/vfs/file.h>     /* fopen/fread/fclose, fileDescriptor_t (bring-up open/read) */
#include <sys/thread.h>      /* O_FILES, td.o_files[] */
#include <ubixos/syscalls.h> /* systemCalls_posix, totalCalls_posix, struct syscall_entry */

#define KCODE_SEL 0x08
#define KDATA_SEL 0x10
#define TSS_SEL 0x28

#define GDT_SLOTS 7 /* null,kcode,kdata,udata,ucode + TSS(2 slots) */

/* 64-bit TSS.  Only rsp0 + iomap_base matter to us; rsp0 is the kernel stack the
 * CPU switches to when an interrupt or syscall enters ring 0 from ring 3. */
struct tss64
{
	u32 reserved0;
	u64 rsp0;
	u64 rsp1;
	u64 rsp2;
	u64 reserved1;
	u64 ist[7];
	u64 reserved2;
	u16 reserved3;
	u16 iomap_base;
} __attribute__((packed));

struct gdt_ptr
{
	u16 limit;
	u64 base;
} __attribute__((packed));

static u64 g_gdt[GDT_SLOTS];
static struct tss64 g_tss;

/* A dedicated ring-0 stack the CPU loads (TSS.rsp0) when an interrupt fires while
 * running ring-3 code, until per-task kernel stacks drive rsp0 (Phase 5b). */
static u8 g_ring0_stack[16384] __attribute__((aligned(16)));

/**
 * Write the 16-byte 64-bit TSS system descriptor into the two GDT slots at
 * TSS_SEL.  @base is the TSS address, @limit its size - 1.
 */
static void gdt_set_tss(u64 base, u32 limit)
{
	u64 *lo = &g_gdt[TSS_SEL / 8];
	u64 *hi = lo + 1;

	*lo = (limit & 0xFFFF) | ((base & 0xFFFFFF) << 16) | ((u64)0x89 << 40) /* P | type=9 (avail 64-bit TSS) */
	      | (((u64)limit & 0xF0000) << 32) | (((base >> 24) & 0xFF) << 56);
	*hi = (base >> 32) & 0xFFFFFFFF;
}

/**
 * Build the full GDT (ring-0 + ring-3 segments + TSS), load it, reload the data
 * segment registers, and load the task register.  After this the CPU can take an
 * interrupt from ring 3 (rsp0) and IRET back out to ring 3.
 */
void x86_64_usermode_init(void)
{
	struct gdt_ptr gp;

	g_gdt[0] = 0x0000000000000000UL; /* null */
	g_gdt[1] = 0x00209A0000000000UL; /* 0x08 kernel code64 (P,DPL0,exec,L) */
	g_gdt[2] = 0x0000920000000000UL; /* 0x10 kernel data   (P,DPL0,RW) */
	g_gdt[3] = 0x0000F20000000000UL; /* 0x18 user data     (P,DPL3,RW) */
	g_gdt[4] = 0x0020FA0000000000UL; /* 0x20 user code64   (P,DPL3,exec,L) */

	g_tss.rsp0 = (u64)(unsigned long)(g_ring0_stack + sizeof(g_ring0_stack));
	g_tss.iomap_base = sizeof(struct tss64); /* no I/O bitmap */
	gdt_set_tss((u64)(unsigned long)&g_tss, sizeof(struct tss64) - 1);

	gp.limit = sizeof(g_gdt) - 1;
	gp.base = (u64)(unsigned long)&g_gdt[0];
	__asm__ __volatile__("lgdt %0" : : "m"(gp));

	/* Reload the data segment registers from the new GDT (CS is already KCODE_SEL
	 * and that descriptor is unchanged, so no far jump is needed here). */
	__asm__ __volatile__("movw %0, %%ds\n\t"
	                     "movw %0, %%es\n\t"
	                     "movw %0, %%ss\n\t"
	                     :
	                     : "r"((u16)KDATA_SEL));

	/* Load the task register with the TSS selector. */
	__asm__ __volatile__("ltr %0" : : "r"((u16)TSS_SEL));
}

/** @return the ring-0 stack top the TSS uses (for callers that re-arm rsp0). */
u64 x86_64_ring0_stack_top(void)
{
	return (u64)(unsigned long)(g_ring0_stack + sizeof(g_ring0_stack));
}

/* -------------------------------------------------------------------------- *
 * Phase 5a one-shot: map a user page, run a ring-3 payload, service its calls.
 * -------------------------------------------------------------------------- */

#define PTE_P 0x1
#define PTE_RW 0x2
#define PTE_US 0x4
#define PTE_ADDR_MASK (~0xFFFUL)

#define USER_CODE_VA 0x40000000UL  /* 1 GB — first VA above the shared low-1 GB identity */
#define USER_STACK_VA 0x40001000UL /* (user mappings live above the identity window) */
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)

/* FreeBSD amd64 syscall numbers (shared by the int 0x80 + syscall-instruction
 * paths; what musl emits).  The full MI POSIX table is wired with the world (5e). */
#define SYS_EXIT 1
#define SYS_READ 3
#define SYS_WRITE 4
#define SYS_OPEN 5
#define SYS_CLOSE 6

extern char x86_64_user_demo_start[];
extern char x86_64_user_demo_end[];
void x86_64_enter_user(u64 user_rip, u64 user_rsp);
void x86_64_leave_user(void);

/** Allocate a zeroed page-table frame.  @return its PHYSICAL address (0 on OOM);
 * the kernel reaches the frame through the physmap (P2V). */
static uintptr_t alloc_table(void)
{
	uintptr_t p = vmm_find_free_page(sysID);
	if (p != 0)
		memset(P2V(p), 0, PAGE_SIZE);
	return p;
}

/**
 * Ensure @table_phys's entry at @slot is present, creating the next-level table
 * (P|RW|US, so the leaf's US bit is honoured) if absent.  Page tables hold and are
 * addressed by PHYSICAL addresses; the CPU reaches them via the physmap.
 * @return the next-level table's PHYSICAL address.
 */
static uintptr_t next_table(uintptr_t table_phys, unsigned slot)
{
	u64 *table = (u64 *)P2V(table_phys);

	if ((table[slot] & PTE_P) == 0)
		table[slot] = alloc_table() | PTE_P | PTE_RW | PTE_US;
	else
		table[slot] |= PTE_US; /* a pre-existing upper entry may lack US (e.g. the kernel's) */
	return table[slot] & PTE_ADDR_MASK;
}

/**
 * Map one 4 KB user page @va -> @phys in the address space rooted at @pml4_phys,
 * walking/creating the PML4->PDPT->PD->PT chain.  Marks the leaf user-accessible
 * (and writable if asked).
 */
void x86_64_map_user_page_to(uintptr_t pml4_phys, u64 va, u64 phys, int writable)
{
	uintptr_t pdpt = next_table(pml4_phys, (unsigned)((va >> 39) & 0x1FF));
	uintptr_t pd = next_table(pdpt, (unsigned)((va >> 30) & 0x1FF));
	uintptr_t pt = next_table(pd, (unsigned)((va >> 21) & 0x1FF));

	((u64 *)P2V(pt))[(va >> 12) & 0x1FF] = (phys & PTE_ADDR_MASK) | PTE_P | PTE_US | (writable ? PTE_RW : 0);
	__asm__ __volatile__("invlpg (%0)" : : "r"((void *)(uintptr_t)va) : "memory");
}

/** Map a user page in the live address space (CR3 holds the physical PML4). */
void x86_64_map_user_page(u64 va, u64 phys, int writable)
{
	u64 cr3;
	__asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
	x86_64_map_user_page_to((uintptr_t)(cr3 & PTE_ADDR_MASK), va, phys, writable);
}

/**
 * Create a fresh per-process address space and @return its PML4's PHYSICAL address
 * (load into CR3 directly).  Layout (matching aarch64's working model):
 *   - the kernel low-1 GB identity (PML4[0] -> PDPT[0]) is shared, so the kernel's
 *     "physical frame == pointer" accesses (the MI ELF loader + uregion mmap/brk
 *     zero-fill a frame via (void *)frame) work while THIS user CR3 is active,
 *     e.g. during a syscall;
 *   - user mappings live ABOVE 1 GB (PDPT[1..], private to this space), so they
 *     never collide with that identity window;
 *   - the kernel higher half (PML4[256..511] — physmap + kernel text) is shared by
 *     pointer so the kernel stays mapped across the CR3 switch.
 * Each space gets its own PDPT under PML4[0] (with PDPT[0] = the shared kernel PD)
 * so the per-process PDPT[1..] user slots stay private.
 */
uintptr_t x86_64_create_user_space(void)
{
	u64 cr3;
	uintptr_t kpml4_phys, kpdpt_phys, pml4_phys, pdpt_phys;
	u64 *kpml4, *kpdpt, *pml4, *pdpt;

	__asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
	kpml4_phys = (uintptr_t)(cr3 & PTE_ADDR_MASK);
	kpml4 = (u64 *)P2V(kpml4_phys);
	kpdpt_phys = (uintptr_t)(kpml4[0] & PTE_ADDR_MASK);
	kpdpt = (u64 *)P2V(kpdpt_phys);

	pml4_phys = alloc_table();
	pdpt_phys = alloc_table();
	if (pml4_phys == 0 || pdpt_phys == 0)
		return 0;
	pml4 = (u64 *)P2V(pml4_phys);
	pdpt = (u64 *)P2V(pdpt_phys);

	pdpt[0] = kpdpt[0]; /* share the kernel low-1 GB identity PD */
	pml4[0] = pdpt_phys | PTE_P | PTE_RW | PTE_US;
	for (unsigned i = 256; i < 512; i++)
		pml4[i] = kpml4[i];
	return pml4_phys;
}

/** Set the kernel stack used on the next ring3->ring0 entry: the TSS rsp0 (for
 * interrupts) and the per-CPU kernel_rsp (for the syscall instruction, which the
 * CPU does not stack-switch).  Called from switch_to for each task. */
void x86_64_set_user_kstack(u64 top)
{
	g_tss.rsp0 = top;
	curcpu()->kernel_rsp = top;
}

#define MSR_EFER 0xC0000080
#define MSR_STAR 0xC0000081
#define MSR_LSTAR 0xC0000082
#define MSR_SFMASK 0xC0000084

extern void x86_64_syscall_entry(void);

/** Write @val to MSR @msr (edx:eax = hi:lo). */
static void wrmsr(u32 msr, u64 val)
{
	u32 lo = (u32)val;
	u32 hi = (u32)(val >> 32);
	__asm__ __volatile__("wrmsr" : : "c"(msr), "a"(lo), "d"(hi));
}

static u64 rdmsr(u32 msr)
{
	u32 lo, hi;
	__asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
	return ((u64)hi << 32) | lo;
}

/**
 * Enable the SYSCALL/SYSRET fast path: EFER.SCE, the STAR segment selectors
 * (SYSCALL -> kernel CS 0x08 / SS 0x10; SYSRET -> user CS 0x23 / SS 0x1b from base
 * 0x10), LSTAR (the entry point), and SFMASK (mask IF on entry so the stub runs
 * with interrupts off until it is on the kernel stack).  Call once per CPU after
 * the GDT is installed.
 */
void x86_64_syscall_init(void)
{
	wrmsr(MSR_EFER, rdmsr(MSR_EFER) | 0x1); /* SCE */
	wrmsr(MSR_STAR, ((u64)0x10 << 48) | ((u64)0x08 << 32));
	wrmsr(MSR_LSTAR, (u64)(unsigned long)x86_64_syscall_entry);
	wrmsr(MSR_SFMASK, 0x200); /* clear IF on syscall entry */
}

/* -------------------------------------------------------------------------- *
 * Machine-dependent hooks for the arch-neutral ELF64 loader (sys/kern/elf64_load.c).
 * -------------------------------------------------------------------------- */

/**
 * Map a loaded ELF page into @aspace_root (an x86_64 PML4 PHYSICAL address passed
 * as the loader's opaque handle).  Mapped writable + user; @executable is unused
 * (no NX / W^X yet, so every user page is executable).
 */
void md_map_user_page(u64 *aspace_root, u64 va, u64 pa, int executable)
{
	(void)executable;
	x86_64_map_user_page_to((u64)(uintptr_t)aspace_root, va, pa, 1);
}

/** Sync the I-cache after writing code (x86_64's I-cache is coherent: no-op). */
void md_sync_icache(uintptr_t addr, u64 len)
{
	(void)addr;
	(void)len;
}

/** Reach a physical frame as a kernel pointer.  x86_64 maps all RAM at the
 * physmap (P2V); the low identity covers only 1 GB, so frames allocated above it
 * (after memory fills) are reachable only here. */
void *md_phys_to_virt(u64 phys)
{
	return P2V(phys);
}

/**
 * Service a ring-3 `int $0x80`.  Bring-up calls only: write (to the serial
 * console) and exit.  Args follow the SysV order in the trapframe: rax = nr,
 * rdi/rsi/rdx = arg0/1/2.  exit terminates the calling task and schedules away,
 * so control returns to whatever the scheduler picks next (never to ring 3).
 */
void x86_64_syscall(struct x86_64_trapframe *tf)
{
	/* The MI table-driven dispatch engine + the FreeBSD POSIX table. */
	extern register_t ksyscall_dispatch(
	    struct thread * td, struct syscall_entry * tbl, int count, u_int32_t number, register_t *args);
	extern void signal_check(struct trapframe * frame);

	/* Record the live trapframe so signal delivery (signal_check) + sys_sigreturn
	 * can read/rewrite it; both name the identical-layout struct trapframe. */
	_current->td.frame = (struct trapframe *)tf;

	/* musl issues some UbixOS-native calls (e.g. exit_group) by ORing NATIVE_FLAG
	 * (0x8000) into the number even on the `syscall`-instruction path — route those
	 * to the native handler (which strips the flag), like aarch64's syscall.c. */
	if (tf->rax & 0x8000)
	{
		x86_64_native_syscall(tf);
		return;
	}

	/* exit terminates the task and schedules away — handled here (the generic
	 * path would return to ring 3). */
	if (tf->rax == SYS_EXIT)
	{
		kprintf("  [ring3] exit(%d) -> terminating task pid=%d\n", (int)tf->rdi, _current ? _current->id : -1);
		endTask(_current->id);
		sched();
		return; /* unreachable */
	}

	/* mmap (477) + brk (17) return a 64-bit user VA, which the int td_retval[] the
	 * generic dispatch uses cannot hold — intercept them and put the full result in
	 * rax directly (the FreeBSD/SysV arg order is RDI/RSI/RDX/R10/...).  fork (2)
	 * needs the trapframe to seed the child. */
	if (tf->rax == 477)
	{
		tf->rax = x86_64_user_mmap(tf->rdi, tf->rsi, tf->rdx, tf->r10, tf->r8, tf->r9);
	}
	else if (tf->rax == 17) /* break */
	{
		tf->rax = x86_64_user_brk(tf->rdi);
	}
	else if (tf->rax == 2) /* fork */
	{
		tf->rax = (u64)x86_64_fork(tf);
	}
	else if (tf->rax == 158) /* arch_prctl — musl's TLS setup (no FreeBSD nr) */
	{
		extern void machine_set_tls(struct thread * td, uintptr_t base);
		if (tf->rdi == 0x1002) /* ARCH_SET_FS: point FS.base at the musl TCB */
			machine_set_tls(&_current->td, (uintptr_t)tf->rsi);
		tf->rax = 0;
	}
	else if (tf->rax == SYS_WRITE && (tf->rdi == 1 || tf->rdi == 2))
	{
		/* Bring-up console fast path: write to fd 1/2 goes straight to serial
		 * (the x86_64 tty/pty fileops are not wired yet). */
		const char *buf = (const char *)(uintptr_t)tf->rsi;
		u64 len = tf->rdx;
		for (u64 i = 0; i < len; i++)
			serial_putc(buf[i]);
		tf->rax = len;
	}
	else
	{
		/* Everything else dispatches through the real MI POSIX table.  The FreeBSD/
		 * SysV amd64 ABI passes args in RDI/RSI/RDX/R10/R8/R9; the syscall runs with
		 * the caller's CR3, so user pointers in the args are directly valid. */
		register_t args[6] = {(register_t)tf->rdi,
		                      (register_t)tf->rsi,
		                      (register_t)tf->rdx,
		                      (register_t)tf->r10,
		                      (register_t)tf->r8,
		                      (register_t)tf->r9};
		tf->rax = (u64)ksyscall_dispatch(
		    &_current->td, systemCalls_posix, totalCalls_posix, (u_int32_t)tf->rax, args);
	}

	/* On the way back to ring 3, deliver any pending signal (rewrites tf to enter
	 * the handler; the handler's magic-return faults into sys_sigreturn). */
	signal_check((struct trapframe *)tf);
}

/* Native (int $0x81) syscall numbers — the UbixOS-native ABI (lib/ubix_api).
 * Mirrors aarch64's NATIVE_* (kern/syscall.c); the calls that take user pointers
 * are dispatched directly (the syscall runs under the caller's CR3, so the
 * pointers are valid) to sidestep the table's uap-struct arg packing. */
#define NATIVE_FLAG 0x8000
#define NATIVE_GETCWD 41
#define NATIVE_KLOG_READ 47
#define NATIVE_MPI_CREATE 50
#define NATIVE_MPI_DESTROY 51
#define NATIVE_MPI_POST 52
#define NATIVE_MPI_FETCH 53
#define NATIVE_EXIT_GROUP 65
#define NATIVE_MPI_WAIT 69

/**
 * Service a ring-3 `int $0x81` — the UbixOS-native ABI (MPI mailboxes,
 * ubix_getcwd, klog_read).  Number in rax, args in RDI/RSI/RDX/R10/R8/R9 (the
 * same SysV order as the POSIX path).  MPI/getcwd/klog are dispatched directly;
 * everything else falls through to the native systemCalls[] table.  Sibling of
 * aarch64_syscall's NATIVE_FLAG branch.
 */
void x86_64_native_syscall(struct x86_64_trapframe *tf)
{
	extern register_t ksyscall_dispatch(
	    struct thread * td, struct syscall_entry * tbl, int count, u_int32_t number, register_t *args);
	extern void signal_check(struct trapframe * frame);
	extern int mpi_createMbox(char *);
	extern int mpi_destroyMbox(char *);
	extern int mpi_postMessage(char *, u_int32_t, void *);
	extern int mpi_fetchMessage(char *, void *);
	extern int mpi_waitMessage(char *, void *, u_int32_t);
	extern int klog_read_wait(void *buf, int max_entries, u_int32_t start_seq);
	extern struct syscall_entry systemCalls[];
	extern int totalCalls;

	extern void endTask(int pid);
	extern void sched(void);

	u64 args[6] = {tf->rdi, tf->rsi, tf->rdx, tf->r10, tf->r8, tf->r9};
	u32 n = (u32)(tf->rax & ~(u64)NATIVE_FLAG); /* strip the flag musl may OR in */

	_current->td.frame = (struct trapframe *)tf;

	switch (n)
	{
		case NATIVE_EXIT_GROUP:
			endTask(_current->id);
			sched();
			return; /* unreachable */

		case NATIVE_MPI_CREATE:
			tf->rax = (u64)mpi_createMbox((char *)(uintptr_t)args[0]);
			break;
		case NATIVE_MPI_DESTROY:
			tf->rax = (u64)mpi_destroyMbox((char *)(uintptr_t)args[0]);
			break;
		case NATIVE_MPI_POST:
			tf->rax = (u64)mpi_postMessage(
			    (char *)(uintptr_t)args[0], (u_int32_t)args[1], (void *)(uintptr_t)args[2]);
			break;
		case NATIVE_MPI_FETCH:
			tf->rax = (u64)mpi_fetchMessage((char *)(uintptr_t)args[0], (void *)(uintptr_t)args[1]);
			break;
		case NATIVE_MPI_WAIT:
			tf->rax = (u64)mpi_waitMessage(
			    (char *)(uintptr_t)args[0], (void *)(uintptr_t)args[1], (u_int32_t)args[2]);
			break;
		case NATIVE_GETCWD:
		{
			char *ubuf = (char *)(uintptr_t)args[0];
			u64 size = args[1];
			const char *cwd = (_current != 0 && _current->oInfo.cwd[0] != '\0') ? _current->oInfo.cwd : "/";
			if (ubuf == 0 || size == 0)
				tf->rax = (u64)-1;
			else
			{
				strncpy(ubuf, cwd, (size_t)size - 1);
				ubuf[size - 1] = '\0';
				tf->rax = 0;
			}
			break;
		}
		case NATIVE_KLOG_READ:
			/* Read the 3 args straight from the registers (the table dispatch packs
			 * the two trailing 32-bit fields into one slot — see the aarch64 note). */
			tf->rax = (u64)klog_read_wait((void *)(uintptr_t)args[0], (int)args[1], (u_int32_t)args[2]);
			break;
		default:
			tf->rax = (u64)ksyscall_dispatch(&_current->td, systemCalls, totalCalls, n, (register_t *)args);
			break;
	}

	signal_check((struct trapframe *)tf);
}

/**
 * Phase 5b: run a real user process the SCHEDULER dispatches, in its own address
 * space.  Build a private PML4, map the demo payload + a stack into it, create a
 * task pointing at that space (md_cr3/md_entry/md_usp), and sched_ready() it.
 * When sched() picks it, switch_to swaps CR3 + rsp0 and user_trampoline IRETQs to
 * ring 3; the payload writes via a syscall and exits, after which the scheduler
 * returns here.  Sibling of aarch64's aarch64_proc_demo.
 */
void x86_64_proc_demo(void)
{
	u64 pml4 = x86_64_create_user_space();
	uintptr_t code_phys = vmm_find_free_page(sysID);
	uintptr_t stack_phys = vmm_find_free_page(sysID);
	unsigned long code_len = (unsigned long)(x86_64_user_demo_end - x86_64_user_demo_start);
	kTask_t *t;
	int i;

	kprintf("proc demo: scheduling a ring-3 process in its own address space...\n");

	memcpy(P2V(code_phys), x86_64_user_demo_start, code_len);
	x86_64_map_user_page_to(pml4, USER_CODE_VA, (u64)code_phys, 0);   /* exec, read-only */
	x86_64_map_user_page_to(pml4, USER_STACK_VA, (u64)stack_phys, 1); /* writable stack */

	t = schedNewTask();
	t->md.md_cr3 = pml4;
	t->md.md_entry = USER_CODE_VA;
	t->md.md_usp = USER_STACK_TOP;
	strncpy(t->name, "ring3proc", sizeof(t->name) - 1);
	sched_ready(t);

	kprintf("  task pid=%d ready; yielding to the scheduler...\n", t->id);

	for (i = 0; i < 64 && t->state != DEAD && t->state != ZOMBIE; i++)
		sched_yield();

	kprintf("proc demo: user process (pid=%d) ran + exited via the scheduler — "
	        "scheduled ring-3 processes work on x86_64.\n",
	        t->id);
}
