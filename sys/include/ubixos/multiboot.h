#ifndef _UBIXOS_MULTIBOOT_H
#define _UBIXOS_MULTIBOOT_H

#include <sys/types.h>

/*
 * Multiboot 1 info structure passed by GRUB in %ebx.
 * Only fields gated by the corresponding flags bit are valid.
 */
struct multiboot_info {
  uint32_t flags;         /* which fields below are valid            */
  uint32_t mem_lower;     /* bit 0: KB of lower memory               */
  uint32_t mem_upper;     /* bit 0: KB of upper memory               */
  uint32_t boot_device;   /* bit 1: BIOS drive + partition           */
  uint32_t cmdline;       /* bit 2: physical addr of command line    */
  uint32_t mods_count;    /* bit 3: number of boot modules           */
  uint32_t mods_addr;     /* bit 3: physical addr of modules array   */
  uint32_t syms[4];       /* bits 4/5: symbol table info             */
  uint32_t mmap_length;   /* bit 6: bytes in memory map              */
  uint32_t mmap_addr;     /* bit 6: physical addr of memory map      */
};

/*
 * boot_device field layout (when flags bit 1 is set):
 *
 *   bits 31-24  BIOS drive number (0x80 = first HD, 0x81 = second HD, ...)
 *   bits 23-16  top-level partition (0-based; 0xFF = not partitioned)
 *   bits 15-8   sub-partition      (0xFF = none)
 *   bits  7-0   sub-sub-partition  (0xFF = none)
 *
 * Mapping to UbixOS kernel device numbers:
 *   BIOS 0x80 → initHardDisk probes 0xE0 first → major 1
 *   BIOS 0x81 → initHardDisk probes 0xF0 second → major 2
 *   Partition 0 (first) → device_add minor 1
 */
#define MB_FLAG_BOOT_DEVICE  (1 << 1)

#define MB_BOOT_DRIVE(bd)     (((bd) >> 24) & 0xFF)
#define MB_BOOT_PARTITION(bd) (((bd) >> 16) & 0xFF)

/* Convert BIOS drive → kernel major (1-based) */
static inline int mb_drive_to_major(uint32_t boot_device) {
  uint8_t drive = MB_BOOT_DRIVE(boot_device);
  return (drive >= 0x80) ? (int)(drive - 0x80 + 1) : 1;
}

/* Convert GRUB partition (0-based) → kernel minor (1-based).
 * 0xFF means unpartitioned — treat as first partition. */
static inline int mb_partition_to_minor(uint32_t boot_device) {
  uint8_t part = MB_BOOT_PARTITION(boot_device);
  return (part == 0xFF) ? 1 : (int)(part + 1);
}

#endif /* _UBIXOS_MULTIBOOT_H */
