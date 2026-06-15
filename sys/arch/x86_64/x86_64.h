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

/* boot/main.c — the 64-bit C entry (called from start.S). */
void kmain_x86_64(u32 mb_magic, u32 mb_info);

#endif /* _X86_64_BRINGUP_H */
