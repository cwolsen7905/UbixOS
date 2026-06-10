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
#include <sys/types.h>
#include <lib/kprintf.h>  /* canonical int kprintf(const char *, ...) */
#include <lib/kconsole.h> /* registered-sink console (kconsole_arch_init) */

/* uart.c — PL011 console plumbing + the PL011 kconsole serial sink.  kprintf
 * itself is the shared arch-neutral entry point in sys/lib/kprintf.c. */
void uart_putc(char c);
void uart_puts(const char *s);
int uart_getc(void);     /* non-blocking PL011 RX: byte, or -1 if empty */
int uart_rx_ready(void); /* non-zero if a byte is waiting (peek) */

/* console.c — wire the PL011 into the VFS console fileops (g_console_ops etc.). */
struct thread;
void aarch64_console_init(void);
int aarch64_console_setup_fds(struct thread *td);

/* exceptions.c — install the EL1 vector table (defined in vectors.S). */
void aarch64_vbar_init(void);
extern char vectors_el1[]; /* vector table base (vectors.S) */

/* gic.c — GICv2 interrupt controller. */
void gic_init(void);
void gic_enable_intid(unsigned intid);
int aarch64_irq_dispatch(void); /* EL1 IRQ vector; returns non-zero on a timer tick */

/* timer.c — ARM generic timer (EL1 physical, PPI 30). */
void timer_init(void);
void timer_tick(void);
/* md_uptime (the aarch64 time source) is declared in <ubixos/time.h>. */

/* mmu.c — enable the MMU with a TTBR0 identity map (39-bit VA). */
void aarch64_mmu_init(void);
u_int64_t *aarch64_kernel_l1(void); /* kernel identity L1 root (basis for new address spaces) */

/* context.S / ctxdemo.c — cooperative context switch + its demo. */
void aarch64_ctx_switch(u_int64_t *save_sp, u_int64_t next_sp);
void aarch64_ctx_demo(void);

/* scheddemo.c — drive the generic sched_core/sched_dispatch with real threads. */
void aarch64_sched_demo(void);

/* vmmdemo.c — bring up + exercise the generic physical page allocator. */
void aarch64_vmm_demo(void);

/* pmap.c — 4 KB-granule page-table mapping + its demo. */
int pmap_map_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, u_int64_t attrs);
int pmap_map_user_page(u_int64_t *l1, u_int64_t va, u_int64_t pa, int executable);
u_int64_t *pmap_create_user_space(void);
u_int64_t pmap_extract(u_int64_t *l1, u_int64_t va); /* user VA -> phys (file-backed mmap) */
u_int64_t *pmap_fork_copy(u_int64_t *parent);        /* deep-copy user mappings (fork) */
void pmap_free_user_space(u_int64_t *l1);            /* free a replaced image's user pages + tables (exec) */
void pmap_switch(u_int64_t *l1);

/* vectors.S — a fork child's first-dispatch landing pad (ERETs to EL0). */
void ret_from_fork(void);
void aarch64_pmap_demo(void);
void aarch64_aspace_demo(void);

/* el0.S — drop to EL0 / return from it + the EL0 demo payload. */
void aarch64_enter_el0(u_int64_t entry, u_int64_t ustack);
void aarch64_eret_to_el0(u_int64_t entry, u_int64_t usp); /* scheduled task: no longjmp save */
void aarch64_exec_to_el0(u_int64_t entry,
                         u_int64_t usp,
                         u_int64_t kstack_top); /* execve restart: re-base SP_EL1 + ERET */
void aarch64_el0_return(void);
extern char user_demo_code_start[];
extern char user_demo_code_end[];
extern char fork_demo_code_start[];
extern char fork_demo_code_end[];

/* syscall.c — minimal EL0 syscall dispatch (write/exit/getpid/fork). */
u_int64_t aarch64_syscall(u_int64_t number, u_int64_t *args);

/* proc.c — fork(): child resumes at the caller's EL0 point with a copied space. */
int aarch64_fork(u_int64_t *parent_tf);

/* syscalldemo.c — map a user page, drop to EL0, exercise the SVC path. */
void aarch64_syscall_demo(void);

/* elfdemo.c — load + run an ELF64 binary via the generic loader. */
void aarch64_elf_demo(void);

/* procdemo.c — run a user process dispatched by the scheduler (Phase 13f). */
void aarch64_proc_demo(void);

/* forkdemo.c — a user process that fork()s; both run via the scheduler (13g). */
void aarch64_fork_demo(void);

/* preemptdemo.c — CPU-bound tasks preempted by the timer-driven scheduler. */
void aarch64_preempt_demo(void);

/* userelfdemo.c — load + run a real compiled aarch64 ELF (userland-port spike). */
void aarch64_user_elf_demo(void);

/* muslelfdemo.c — run a musl-linked program (spike U2). */
void aarch64_musl_elf_demo(void);

/* procfsdemo.c — mount procfs + read /proc/meminfo through the generic VFS. */
void aarch64_procfs_demo(void);

/* ramfsdemo.c — mount ramfs + round-trip a file (VFS read + write/create). */
void aarch64_ramfs_demo(void);

/* execfile.c — load + run an ELF image / a file off the VFS (exec-from-file). */
int aarch64_run_elf_image(const void *image, const char *name);
void aarch64_spawn_elf_image(const void *image, const char *name); /* schedule a daemon, no wait */
void aarch64_exec_file(const char *path);
int aarch64_exec_replace(const char *path,
                         char *const *argv,
                         char *const *envp);               /* execve: replace current image, restart EL0 */
int aarch64_wait4(int want_pid, int *status, int options); /* cooperative reap (WNOHANG-aware) */
void aarch64_run_init(const char *path);                   /* run /bin/init + become the idle loop (no return) */
void aarch64_run_dynamic(const char *path);                /* load+run a PIE program, wait (capped) */
void aarch64_run_dynamic_init(const char *path);           /* run a PIE program as the system (no return) */
void aarch64_spawn_dynamic(const char *path);              /* schedule a PIE daemon, return (no wait) */
int aarch64_file_exists(const char *path);                 /* boot profile selector: is @path staged? */

/* virtio_blk.c — virtio-mmio block device (returns a ubx_device with blk ops). */
struct ubx_device;
struct ubx_device *aarch64_virtio_blk_init(void);

/* virtio_net.c — virtio-mmio network device (polling; lwIP bridge calls these). */
int aarch64_virtio_net_init(void);                                    /* scan + bring up; 0 on success */
int virtio_net_send(const void *frame, u_int32_t len);                /* TX one Ethernet frame */
int virtio_net_poll_rx(void (*deliver)(const u_int8_t *, u_int32_t)); /* drain RX used-ring */
extern u_int8_t virtio_net_mac[6];                                    /* device MAC */
extern int virtio_net_ready;                                          /* link-up flag */

/* net/virtio_netif.c — lwIP bridge + aarch64 network bring-up (tcpip + DHCP). */
int aarch64_net_init(void);

/* virtio_gpu.c — virtio-mmio 2D GPU (scanout framebuffer for views/objGFX). */
int aarch64_virtio_gpu_init(void); /* scan + bring up + initial scanout; 0 on success */
void virtio_gpu_flush(void);       /* present the framebuffer (transfer + flush) */
extern u_int8_t *virtio_gpu_fb;    /* linear framebuffer (B8G8R8X8) */
extern u_int32_t virtio_gpu_width;
extern u_int32_t virtio_gpu_height;
extern u_int32_t virtio_gpu_pitch;

/* virtio_input.c — virtio-mmio keyboard/mouse (Linux input events). */
int aarch64_virtio_input_init(void); /* scan + bring up all input devices; count */

/* fbcon.c — kernel framebuffer text console (KC_PRIMARY kconsole sink). */
void aarch64_fbcon_init(void); /* register the on-screen console over virtio-gpu */

/* virtio_sound.c — virtio-mmio PCM playback exposed as /dev/audio. */
int aarch64_virtio_sound_init(void); /* scan + bring up + start a 44.1kHz output stream */
int virtio_input_poll(void (*deliver)(int dev, u_int16_t type, u_int16_t code, u_int32_t value));

#endif /* _AARCH64_BRINGUP_H */
