/*-
 * Copyright (c) 2002-2026 The UbixOS Project.  All rights reserved.
 *
 * x86-64 bring-up internal declarations.  Shared by the standalone bring-up
 * objects under boot/ and kern/ until the port reuses the generic kernel
 * headers.  Reached as "x86_64.h" (the build adds -Isys/arch/x86_64).
 */
#ifndef _X86_64_BRINGUP_H
#define _X86_64_BRINGUP_H

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long u64;

/* Port I/O (shared by the serial console + PIC/PIT). */
static inline void outb(u16 port, u8 val)
{
	__asm__ __volatile__("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline u8 inb(u16 port)
{
	u8 r;
	__asm__ __volatile__("inb %1, %0" : "=a"(r) : "Nd"(port));
	return r;
}

static inline void io_wait(void)
{
	outb(0x80, 0); /* write to an unused port — a short, portable I/O delay */
}

/* console.c — COM1 serial console (the bring-up console). */
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(u64 v);
void serial_putdec(u64 v);

/* idt.c — 64-bit IDT + exception handling (Phase 2). */
void idt_init(void);

/* vmm/vmm_machdep.c — physical page allocator setup (Phase 3). */
void x86_64_mem_init(void);

/* irq.c — 8259 PIC + PIT timer + IRQ dispatch (Phase 4a). */
void pic_remap(void);
void pit_init(unsigned hz);
void irq_eoi(unsigned vector);
void x86_64_irq(unsigned vector); /* called from the common ISR path for vec >= 32 */
u64 timer_ticks(void);

/* boot/main.c — the 64-bit C entry (called from start.S). */
void kmain_x86_64(u32 mb_magic, u32 mb_info);

/* cpu_switch.S — register-level context switch (Phase 4b). */
void x86_64_ctx_switch(u64 *save_rsp, u64 next_rsp);

/* ctxtest.c — verify the context switch (bring-up demo, Phase 4b). */
void x86_64_ctx_test(void);

#endif /* _X86_64_BRINGUP_H */
