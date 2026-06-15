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

/* The kernel is linked at the higher half; a kernel-symbol VMA minus KERNBASE is
 * its physical (load) address.  Must match ldscript.x86_64 + start.S. */
#define KERNBASE 0xFFFFFFFF80000000UL
#define KERN_VIRT_TO_PHYS(v) ((u64)(v) - KERNBASE)

/* Physmap (direct map): start.S maps all RAM at PHYSMAP_BASE (PML4[256]), so the
 * kernel reaches any physical address as a pointer there — the replacement for the
 * low identity map once it is dropped (user space owns the low half).  P2V turns a
 * physical address into a kernel pointer; V2P recovers the physical address (e.g.
 * for a PTE or a device DMA address). */
#define PHYSMAP_BASE 0xFFFF800000000000UL
#define P2V(p) ((void *)((u64)(p) + PHYSMAP_BASE))
#define V2P(v) ((u64)(v) - PHYSMAP_BASE)

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

static inline void outw(u16 port, u16 val)
{
	__asm__ __volatile__("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline u16 inw(u16 port)
{
	u16 r;
	__asm__ __volatile__("inw %1, %0" : "=a"(r) : "Nd"(port));
	return r;
}

static inline void outl(u16 port, u32 val)
{
	__asm__ __volatile__("outl %0, %1" : : "a"(val), "Nd"(port));
}

static inline u32 inl(u16 port)
{
	u32 r;
	__asm__ __volatile__("inl %1, %0" : "=a"(r) : "Nd"(port));
	return r;
}

/* console.c — COM1 serial console (the bring-up console). */
void serial_init(void);
void serial_putc(char c);
void serial_puts(const char *s);
void serial_puthex(u64 v);
void serial_putdec(u64 v);

/* ksupport.c — register COM1 as a kconsole sink (so the real kprintf routes here). */
void x86_64_console_init(void);
int kprintf(const char *fmt, ...);

/* idt.c — 64-bit IDT + exception handling (Phase 2). */
void idt_init(void);

/*
 * Trapframe pushed by the ISR stubs (isr.S): GP registers (r15 at the lowest
 * address), then vector + error code, then the CPU-pushed IRET frame.
 */
struct x86_64_trapframe
{
	u64 rax, rbx, rcx, rdx, rsi, rdi, rbp;
	u64 r8, r9, r10, r11, r12, r13, r14, r15;
	u64 vector, error;
	u64 rip, cs, rflags, rsp, ss;
};

void x86_64_exception(struct x86_64_trapframe *tf);

/* usermode.c — ring-3 GDT + TSS, per-process address spaces, syscalls (5a/5b). */
void x86_64_usermode_init(void);
u64 x86_64_ring0_stack_top(void);
void x86_64_map_user_page(u64 va, u64 phys, int writable);
void x86_64_map_user_page_to(u64 pml4_phys, u64 va, u64 phys, int writable);
u64 x86_64_create_user_space(void);
void x86_64_set_user_kstack(u64 top);
void x86_64_syscall_init(void); /* enable SYSCALL/SYSRET (STAR/LSTAR/SFMASK/EFER.SCE) */
void x86_64_syscall(struct x86_64_trapframe *tf);

/* syscall_md.c — mmap/brk return a 64-bit VA directly (td_retval is 32-bit), so
 * the syscall entry intercepts them.  See x86_64_syscall. */
u64 x86_64_user_mmap(u64 addr, u64 len, u64 prot, u64 flags);
u64 x86_64_user_brk(u64 newbrk);

/* execfile.c — load an ELF64 + build the SysV initial stack (Phase 5e). */
u64 x86_64_build_user_image(const void *image, u64 *out_entry, u64 *out_usp);
void x86_64_exec_demo(const char *path); /* load + run a real on-disk ELF64 */
void x86_64_user_demo(void);             /* 5a one-shot (kept; superseded by proc_demo) */
void x86_64_proc_demo(void);             /* 5b scheduled ring-3 process */
void x86_64_elf_demo(void);              /* 5d-C: load a synthesized ELF64 via the MI loader */

/* userentry.S — ring-3 entry primitives. */
void x86_64_enter_user(u64 user_rip, u64 user_rsp); /* coroutine (5a one-shot) */
void x86_64_leave_user(void);
void x86_64_iret_to_user(u64 user_rip, u64 user_rsp); /* bare IRETQ (scheduled tasks) */

/* dev/virtio_blk.c — virtio-blk over legacy virtio-pci (Phase 5c). */
struct ubx_device;
int virtio_blk_init(void);
int virtio_blk_read(struct ubx_device *dev, u32 lba, u32 count, void *buf);
int virtio_blk_write(struct ubx_device *dev, u32 lba, u32 count, void *buf);

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
