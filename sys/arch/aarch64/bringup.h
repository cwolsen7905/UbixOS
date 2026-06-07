/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * Shared prototypes for the AArch64 bring-up (QEMU `virt`).  This is the minimal
 * standalone bring-up surface (UART/kprintf, exception vectors, GIC, timer); it
 * is replaced/absorbed by the real kernel interfaces as aarch64 matures.
 */
#ifndef _AARCH64_BRINGUP_H
#define _AARCH64_BRINGUP_H

#include <stdarg.h>
#include <stdint.h>

/* uart.c — PL011 console + a minimal kprintf. */
void uart_putc(char c);
void uart_puts(const char *s);
void kvprintf(const char *fmt, va_list ap);
void kprintf(const char *fmt, ...);

/* exceptions.c — install the EL1 vector table (defined in vectors.S). */
void aarch64_vbar_init(void);
extern char vectors_el1[]; /* vector table base (vectors.S) */

/* gic.c — GICv2 interrupt controller. */
void gic_init(void);
void gic_enable_intid(unsigned intid);
void aarch64_irq_dispatch(void); /* called from the EL1 IRQ vector */

/* timer.c — ARM generic timer (EL1 physical, PPI 30). */
void timer_init(void);
void timer_tick(void);

/* mmu.c — enable the MMU with a TTBR0 identity map (39-bit VA). */
void aarch64_mmu_init(void);

/* context.S / ctxdemo.c — cooperative context switch + its demo. */
void aarch64_ctx_switch(uint64_t *save_sp, uint64_t next_sp);
void aarch64_ctx_demo(void);

#endif /* _AARCH64_BRINGUP_H */
