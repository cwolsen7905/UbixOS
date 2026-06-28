/*-
 * Copyright (c) 2002-2026 The UbixOS Project.
 * All rights reserved.
 *
 * QEMU `virt` board definition + the active-board pointer (M1 board abstraction,
 * docs/design/raspberry-pi-3b-bringup.md).  This is the default board; g_board is
 * reselected from the DTB /compatible string for real hardware (Raspberry Pi 3,
 * Allwinner H618) as the port matures.  Bases are physical (drivers add
 * PHYSMAP_BASE).
 */

#include "bringup.h"

#ifndef BOARD_RPI3 /* QEMU is the default board; the Pi build (board_rpi3.c) owns g_board */

static struct aarch64_board g_board_qemu_virt = {
    .name = "qemu-virt",
    .ram_base = 0x40000000UL,
    .uart_base = 0x09000000UL, /* PL011 UART0 */
    .gicd_base = 0x08000000UL, /* GICv2 distributor   */
    .gicc_base = 0x08010000UL, /* GICv2 CPU interface */
    .intc = &g_gicv2_intc,     /* GICv2 (gic.c)       */
};

/* The active board.  Statically the QEMU default so the very first kprintf works
 * before any DTB-driven reselection. */
struct aarch64_board *g_board = &g_board_qemu_virt;

#endif /* !BOARD_RPI3 */
