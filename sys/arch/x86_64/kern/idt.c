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

#define KERNEL_CS 0x08 /* 64-bit code selector (start.S GDT) */
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
struct x86_64_trapframe
{
	u64 rax, rbx, rcx, rdx, rsi, rdi, rbp;
	u64 r8, r9, r10, r11, r12, r13, r14, r15;
	u64 vector, error;
	u64 rip, cs, rflags, rsp, ss;
};

static struct idt_gate g_idt[NIDT];
static struct idt_ptr g_idt_ptr;

extern void *isr_stub_table[32]; /* isr.S */

static void idt_set_gate(int vec, void *handler)
{
	u64 addr = (u64)handler;
	g_idt[vec].off_lo = (u16)(addr & 0xFFFF);
	g_idt[vec].selector = KERNEL_CS;
	g_idt[vec].ist = 0;
	g_idt[vec].attr = IDT_GATE_INT;
	g_idt[vec].off_mid = (u16)((addr >> 16) & 0xFFFF);
	g_idt[vec].off_hi = (u32)((addr >> 32) & 0xFFFFFFFF);
	g_idt[vec].zero = 0;
}

/** Build the IDT (32 exception vectors + 16 PIC IRQ vectors) and load it. */
void idt_init(void)
{
	for (int v = 0; v < 48; v++)
		idt_set_gate(v, isr_stub_table[v]);

	g_idt_ptr.limit = (u16)(sizeof(g_idt) - 1);
	g_idt_ptr.base = (u64)&g_idt[0];
	__asm__ __volatile__("lidt %0" : : "m"(g_idt_ptr));
}

static const char *const g_exc_names[32] = {
    "#DE divide error",      "#DB debug",
    "NMI",                   "#BP breakpoint",
    "#OF overflow",          "#BR bound range",
    "#UD invalid opcode",    "#NM device n/a",
    "#DF double fault",      "coproc seg overrun",
    "#TS invalid TSS",       "#NP segment n/p",
    "#SS stack fault",       "#GP general protection",
    "#PF page fault",        "reserved(15)",
    "#MF x87 FP error",      "#AC alignment check",
    "#MC machine check",     "#XM SIMD FP",
    "#VE virtualization",    "#CP control prot",
    "22", "23", "24", "25", "26", "27", "28", "29", "30", "31"};

/**
 * C exception handler (from isr_common).  Dumps the trapframe and halts — bring-up
 * exceptions are fatal; this just makes them visible.
 */
void x86_64_exception(struct x86_64_trapframe *tf)
{
	u64 cr2;

	/* Vectors 32..47 are hardware IRQs (PIC-remapped) — handle + EOI, then IRETQ. */
	if (tf->vector >= 32)
	{
		x86_64_irq((unsigned)tf->vector);
		return;
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

	for (;;)
		__asm__ __volatile__("cli; hlt");
}
