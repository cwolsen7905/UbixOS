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

#define USER_CODE_VA 0x40000000UL  /* 1 GB: PDPT[1], clear of start.S's 1 GB identity map */
#define USER_STACK_VA 0x40001000UL /* one page above the code */
#define USER_STACK_TOP (USER_STACK_VA + PAGE_SIZE)

/* Bring-up syscall numbers (int 0x80) — replaced by the FreeBSD ABI in 5d/5e. */
#define SYS_WRITE 1
#define SYS_EXIT 2

extern char x86_64_user_demo_start[];
extern char x86_64_user_demo_end[];
void x86_64_enter_user(u64 user_rip, u64 user_rsp);
void x86_64_leave_user(void);

/** Allocate a zeroed page-table frame (identity-mapped, usable as a pointer). */
static u64 *alloc_table(void)
{
	uintptr_t p = vmm_find_free_page(sysID);
	if (p != 0)
		memset((void *)p, 0, PAGE_SIZE);
	return (u64 *)p;
}

/**
 * Ensure the table entry at @slot is present; create the next-level table (with
 * P|RW|US, so the leaf's US bit is honoured) if absent.  @return the next table.
 */
static u64 *next_table(u64 *table, unsigned slot)
{
	if ((table[slot] & PTE_P) == 0)
	{
		u64 *t = alloc_table();
		table[slot] = (u64)(uintptr_t)t | PTE_P | PTE_RW | PTE_US;
	}
	else
	{
		/* The entry may pre-exist without the user bit (start.S builds PML4[0]
		 * with P|RW only).  US is ANDed across all levels, so the upper entries on
		 * the path to a user leaf must have it set; the kernel's own leaves keep
		 * US=0 and stay protected regardless. */
		table[slot] |= PTE_US;
	}
	return (u64 *)(uintptr_t)(table[slot] & PTE_ADDR_MASK);
}

/**
 * Map one 4 KB user page @va -> @phys, walking/creating the PML4->PDPT->PD->PT
 * chain from the live CR3.  @va must not fall inside start.S's 2 MB identity map
 * (use a VA >= 1 GB).  Marks the leaf user-accessible (and writable if asked).
 */
void x86_64_map_user_page(u64 va, u64 phys, int writable)
{
	u64 cr3;
	u64 *pml4, *pdpt, *pd, *pt;

	__asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
	pml4 = (u64 *)(uintptr_t)(cr3 & PTE_ADDR_MASK);
	pdpt = next_table(pml4, (unsigned)((va >> 39) & 0x1FF));
	pd = next_table(pdpt, (unsigned)((va >> 30) & 0x1FF));
	pt = next_table(pd, (unsigned)((va >> 21) & 0x1FF));

	pt[(va >> 12) & 0x1FF] = (phys & PTE_ADDR_MASK) | PTE_P | PTE_US | (writable ? PTE_RW : 0);
	__asm__ __volatile__("invlpg (%0)" : : "r"((void *)(uintptr_t)va) : "memory");
}

/**
 * Service a ring-3 `int $0x80` (Phase 5a).  Bring-up calls only: write (to the
 * serial console) and exit (return to the kernel via x86_64_leave_user()).  args
 * follow the SysV order in the trapframe: rax=nr, rdi/rsi/rdx = arg0/1/2.
 */
void x86_64_syscall(struct x86_64_trapframe *tf)
{
	switch (tf->rax)
	{
		case SYS_WRITE:
		{
			const char *buf = (const char *)(uintptr_t)tf->rsi;
			u64 len = tf->rdx;
			for (u64 i = 0; i < len; i++)
				serial_putc(buf[i]);
			tf->rax = len; /* bytes written */
			break;
		}
		case SYS_EXIT:
			kprintf("  [ring3] exit(%d) -> returning to kernel\n", (int)tf->rdi);
			x86_64_leave_user(); /* does not return */
			break;
		default:
			kprintf("  [ring3] unknown syscall %u\n", (unsigned)tf->rax);
			tf->rax = (u64)-1;
			break;
	}
}

/**
 * One-shot ring-3 proof: map the demo payload + a stack as user pages, drop to
 * ring 3, and let it run.  It writes via the syscall path and exits back here.
 */
void x86_64_user_demo(void)
{
	uintptr_t code_phys = vmm_find_free_page(sysID);
	uintptr_t stack_phys = vmm_find_free_page(sysID);
	unsigned long code_len = (unsigned long)(x86_64_user_demo_end - x86_64_user_demo_start);

	kprintf("user demo: dropping to ring 3 (payload %u bytes @ VA %X)...\n", (unsigned)code_len, USER_CODE_VA);

	memcpy((void *)code_phys, x86_64_user_demo_start, code_len);
	x86_64_map_user_page(USER_CODE_VA, (u64)code_phys, 0);   /* exec (no NX), read-only */
	x86_64_map_user_page(USER_STACK_VA, (u64)stack_phys, 1); /* writable stack */

	x86_64_enter_user(USER_CODE_VA, USER_STACK_TOP);

	kprintf("user demo: back in the kernel — ring 3 + syscall round-trip works on x86_64.\n");
}
