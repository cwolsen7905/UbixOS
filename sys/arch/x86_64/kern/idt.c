/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 IDT + exception handling (long-mode bring-up Phase 2).
 *
 * Builds a 256-entry 64-bit IDT, installs the 32 CPU-exception stubs (isr.S), and
 * provides the C handler that dumps the trapframe over the serial console so any
 * fault is visible instead of a silent triple-fault — the x86_64 analogue of
 * i386's idt.c + aarch64's aarch64_exception.
 */

#include "x86_64.h"
#include <ubixos/sched.h>       /* _current, sched() — ring-3 fault containment */
#include <ubixos/endtask.h>     /* endTask */
#include <sys/trap.h>           /* struct trapframe */
#include <machine/signal.h>     /* X86_64_SIGTRAMP_RETADDR, struct ubx_sigcontext */
#include <sys/sysproto_posix.h> /* struct sys_sigreturn_args, sys_sigreturn */

#define KERNEL_CS 0x08    /* 64-bit code selector (start.S GDT) */
#define IDT_GATE_INT 0x8E /* present, DPL0, 64-bit interrupt gate */
#define NIDT 256

/* 64-bit IDT gate descriptor (16 bytes). */
struct idt_gate
{
	u16 off_lo;
	u16 selector;
	u8 ist;
	u8 attr;
	u16 off_mid;
	u32 off_hi;
	u32 zero;
} __attribute__((packed));

struct idt_ptr
{
	u16 limit;
	u64 base;
} __attribute__((packed));

/*
 * Trapframe — matches the push order in isr_common (isr.S): GP regs (rax at the
 * lowest address), then vector + error code, then the CPU-pushed iret frame.
 */
static struct idt_gate g_idt[NIDT];
static struct idt_ptr g_idt_ptr;

extern void *isr_stub_table[48]; /* isr.S — 32 CPU exceptions + 16 PIC IRQ stubs */
extern void isr_syscall(void);   /* isr.S — the int 0x80 (vector 128) entry */
extern void isr_syscall81(void); /* isr.S — the int 0x81 (vector 129) native-ABI entry */

#define IDT_GATE_USER 0xEE /* present, DPL3, 64-bit interrupt gate (ring 3 may invoke) */

static void idt_set_gate(int vec, void *handler, u8 attr)
{
	u64 addr = (u64)handler;
	g_idt[vec].off_lo = (u16)(addr & 0xFFFF);
	g_idt[vec].selector = KERNEL_CS;
	g_idt[vec].ist = 0;
	g_idt[vec].attr = attr;
	g_idt[vec].off_mid = (u16)((addr >> 16) & 0xFFFF);
	g_idt[vec].off_hi = (u32)((addr >> 32) & 0xFFFFFFFF);
	g_idt[vec].zero = 0;
}

/** Build the IDT (32 exception vectors + 16 PIC IRQ vectors + the syscall gate). */
void idt_init(void)
{
	for (int v = 0; v < 48; v++)
		idt_set_gate(v, isr_stub_table[v], IDT_GATE_INT);

	/* Vector 0x80: DPL3 so ring-3 code can `int $0x80` (the POSIX/FreeBSD syscall). */
	idt_set_gate(0x80, (void *)isr_syscall, IDT_GATE_USER);
	/* Vector 0x81: DPL3 — the UbixOS-native ABI (lib/ubix_api: MPI, ubix_getcwd). */
	idt_set_gate(0x81, (void *)isr_syscall81, IDT_GATE_USER);

	g_idt_ptr.limit = (u16)(sizeof(g_idt) - 1);
	g_idt_ptr.base = (u64)&g_idt[0];
	__asm__ __volatile__("lidt %0" : : "m"(g_idt_ptr));
}

static const char *const g_exc_names[32] = {"#DE divide error",
                                            "#DB debug",
                                            "NMI",
                                            "#BP breakpoint",
                                            "#OF overflow",
                                            "#BR bound range",
                                            "#UD invalid opcode",
                                            "#NM device n/a",
                                            "#DF double fault",
                                            "coproc seg overrun",
                                            "#TS invalid TSS",
                                            "#NP segment n/p",
                                            "#SS stack fault",
                                            "#GP general protection",
                                            "#PF page fault",
                                            "reserved(15)",
                                            "#MF x87 FP error",
                                            "#AC alignment check",
                                            "#MC machine check",
                                            "#XM SIMD FP",
                                            "#VE virtualization",
                                            "#CP control prot",
                                            "22",
                                            "23",
                                            "24",
                                            "25",
                                            "26",
                                            "27",
                                            "28",
                                            "29",
                                            "30",
                                            "31"};

/**
 * C exception handler (from isr_common).  Dumps the trapframe and halts — bring-up
 * exceptions are fatal; this just makes them visible.
 */
void x86_64_exception(struct x86_64_trapframe *tf)
{
	u64 cr2;

	/* Vector 128 (0x80) is the POSIX/FreeBSD syscall gate (DPL3) — dispatch + IRETQ. */
	if (tf->vector == 128)
	{
		x86_64_syscall(tf);
		return;
	}

	/* Vector 129 (0x81) is the UbixOS-native syscall gate (DPL3) — lib/ubix_api's
	 * MPI mailboxes, ubix_getcwd, etc.  Dispatch via the native table + IRETQ. */
	if (tf->vector == 129)
	{
		x86_64_native_syscall(tf);
		return;
	}

	/* Vectors 32..47 are hardware IRQs (PIC-remapped) — handle + EOI, then IRETQ. */
	if (tf->vector >= 32)
	{
		x86_64_irq((unsigned)tf->vector);
		return;
	}

	/* A ring-3 fault on the magic signal-return address means a handler is
	 * returning: restore the interrupted context via sys_sigreturn (the user RSP
	 * points at the saved sigcontext) and IRETQ back into it, rather than faulting. */
	if (tf->vector == 14 && (tf->cs & 3) == 3 && tf->rip == X86_64_SIGTRAMP_RETADDR)
	{
		struct sys_sigreturn_args sra;
		sra.scp = (struct ubx_sigcontext *)(uintptr_t)tf->rsp;
		_current->td.frame = (struct trapframe *)tf;
		sys_sigreturn(&_current->td, &sra);
		return; /* tf now holds the restored context */
	}

	__asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));

	serial_puts("\n*** x86_64 exception: ");
	serial_puts((char *)g_exc_names[tf->vector < 32 ? tf->vector : 31]);
	serial_puts(" (vec=");
	serial_putdec(tf->vector);
	serial_puts(" err=");
	serial_puthex(tf->error);
	serial_puts(")\n");
	serial_puts("  RIP=");
	serial_puthex(tf->rip);
	serial_puts(" CS=");
	serial_puthex(tf->cs);
	serial_puts(" RFLAGS=");
	serial_puthex(tf->rflags);
	serial_puts("\n  RSP=");
	serial_puthex(tf->rsp);
	serial_puts(" RBP=");
	serial_puthex(tf->rbp);
	serial_puts(" CR2=");
	serial_puthex(cr2);
	serial_puts("\n");

	/* Fault-containment: a fault taken in ring 3 (CS RPL == 3) is the user
	 * program's fault, not the kernel's — terminate the task and schedule on, so a
	 * buggy app can't take down the OS (the x86_64 analog of aarch64's EL0->kill). */
	if ((tf->cs & 3) == 3)
	{
		serial_puts("  (ring-3 fault — terminating the task)\n");
		endTask(_current ? _current->id : 0);
		sched();
		/* not reached */
	}

	/* A ring-0 fault while servicing a *user process's* syscall (md_usp != 0 marks a
	 * ring-3 task) is almost always that syscall dereferencing a bad user-supplied
	 * value — recoverable by killing the offending task rather than halting the whole
	 * OS (e.g. a corrupt fork child passing a garbage fd).  The syscall faulted before
	 * mutating shared state in the cases that hit this, so abandoning its kernel stack
	 * is survivable.  A fault with no user task is a genuine kernel bug — halt. */
	if (_current != 0 && _current->md.md_usp != 0)
	{
		serial_puts("  (ring-0 fault in a syscall — terminating the user task)\n");
		endTask(_current->id);
		sched();
		/* not reached */
	}

	for (;;)
		__asm__ __volatile__("cli; hlt");
}
