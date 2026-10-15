# Installer — design + build plan

Goal: **one image people download, boot live, and install from**, Ubuntu-style.
VMs first (QEMU, UTM, VirtualBox), real PCs later.

## Status

| Lane | Item | Row | Status |
|------|------|-----|--------|
| B0 | This plan | — | ✅ |
| B1 | Clean-room UEFI loader, x86_64 | W-054 | 🚧 PR open |
| B2 | Clean-room UEFI loader, aarch64 | W-055 | 🚧 PR open |
| B3 | Ramdisk / initrd live root | W-056 | ⬜ |
| B4 | Live media: `mkmedia` + CI smoke job | W-057 | ⬜ |
| B5 | Disk layer: disk-utility-plan phases 0 + 0b, GPT | W-058 | ⬜ |
| B6 | Disk tools on the device: `ubdisk`, `format`, `ubpool` | W-059 | ⬜ |
| B7 | Text installer: `ubinstall` + `libinstall` | W-060 | ⬜ |
| B8 | Graphical installer on `views` | W-061 | ⬜ |
| B9 | AHCI driver | W-062 | ⬜ |
| B10 | NVMe driver | W-063 | ⬜ |
| B11 | USB mass storage driver | W-064 | ⬜ |

## Defaults (standing; the person may still change them)

- **VMs first.** QEMU, UTM and VirtualBox are the targets until the installer
  works end to end; real PCs follow.
- **UEFI only.** No legacy BIOS / CSM path, on either arch.
- **GPT.** The installed disk and the live media use GPT. MBR stays readable
  (today's images and the Pi), but the installer writes GPT.
- **Text installer before the graphical one.** The graphical installer (B8) is
  a front end on the same `libinstall` the text one (B7) uses.

## Where we start

- x86_64 boots only via QEMU `-kernel` (PVH); aarch64 the same, plus U-Boot on
  the Pi. Nothing boots from firmware off a disk.
- Disk drivers: virtio-blk only (plus SD on the Pi). No ramdisk, no live root.
- `sbin/ubdisk`, `sbin/ubpool`, `sbin/format` exist; `sys/kern/disk_query.c`
  starts the disk-utility data layer
  ([disk-utility-plan.md](disk-utility-plan.md)).
- `tools/mkimage.sh` already builds the target layout on the host: FAT boot +
  swap + UbixFS pool. **The installer does the same thing on the device.**

## Target layout (installed disk)

GPT with three partitions, the same roles `mkimage.sh` uses today:

| # | Partition | Contents |
|---|-----------|----------|
| 1 | EFI System Partition (FAT32) | `\EFI\BOOT\BOOTX64.EFI` / `BOOTAA64.EFI` (our loader), the kernel |
| 2 | swap | raw swap |
| 3 | UbixFS pool | the root `/` |

The kernel finds swap and the pool by GPT partition-type GUID (uBix-specific
GUIDs, chosen in B5), not by MBR type byte.

## Dependencies

```
B1 ──► B2
B1 ─┬► B4 ─┐
B3 ─┘      ├► B7 ──► B8
B5 ──► B6 ─┘
B5 ──► B9, B10, B11
```

B1, B3 and B5 can start at once and run in parallel. B9–B11 are not on the
VM-first critical path (QEMU and UTM give virtio-blk), but VirtualBox and real
PCs need B9 (AHCI) or B10 (NVMe), and installing from a USB stick needs B11.

## Phases

### B1 — UEFI loader, x86_64

*Implemented (W-054):* `stand/efi/` (MI loader + `x86_64/` hand-off),
`sys/include/ubixos/bootinfo.h`, `share/mk/ubix.efi.mk`, `bmake efi`,
`tools/run-uefi-x86_64.sh`. The loader drops to 32-bit protected mode and
enters the kernel's existing physical `_start` with `eax = UBIX_BOOTINFO_MAGIC`,
`ebx = &bootinfo`; the kernel tells UEFI from PVH by the magic
(`x86_64_mem_init`, `x86_64_fb_init`). Kernel found at `\boot\kernel\kernel`
on the ESP. Known gap: ACPI RSDP is not yet consumed by `madt.c` (it scans the
BIOS areas), so SMP enumeration falls back to the BSP under OVMF.

A small EFI application (PE32+) written from the UEFI specification: open the
ESP through the Simple File System protocol, load the kernel ELF, get the
memory map and the GOP framebuffer, find the ACPI RSDP in the configuration
table, load the B3 ramdisk if present, `ExitBootServices`, and jump to the
kernel with a uBix boot-info block. The kernel gains a UEFI entry next to the
PVH one; PVH stays for `bmake run-*`. Done when QEMU + OVMF boots a disk image
to login with no `-kernel`.

### B2 — UEFI loader, aarch64

*Implemented (W-055):* `stand/efi/aarch64/` (MD part: DTB lookup from the
firmware configuration table, cache clean, MMU off at the current EL, enter the
kernel's `_start` with `x0 = DTB`, `x1 = bootinfo`, `x2 = UBIX_BOOTINFO_MAGIC`),
`tools/run-aavmf-aarch64.sh`, `bmake efi` / `bmake run-aavmf-aarch64`. The kernel
is unchanged (it already takes `x0 = DTB`). Known gaps: the run script uses
`acpi=off` because AAVMF hides the DTB when ACPI is on and the aarch64 kernel
has no ACPI consumer; a full login-prompt smoke needs a built image
(`bmake world image`) and has not been run yet.

The same loader built for AArch64 (`BOOTAA64.EFI`), sharing the MI loader code;
the MD part is the entry state the kernel expects (EL, MMU off, DTB or ACPI
pointer). Done when QEMU `virt` + AAVMF (edk2) and UTM boot a disk image to
login. The Pi keeps U-Boot.

### B3 — Ramdisk / initrd live root

A memory-backed block device the loader hands over, mounted as `/`, so the
live system runs without touching any disk. Works the same on both arches and
from both PVH (`-initrd`) and UEFI. Done when both arches boot to login from
the ramdisk alone with no disk attached.

### B4 — Live media

`tools/mkmedia.sh` (and `bmake media`) builds the downloadable image: GPT, an
ESP with the loader, kernel and ramdisk, and the install payload. A CI job
boots it headless under QEMU + OVMF / AAVMF on both arches and checks for the
login prompt. Done when the job is green on both arches.

### B5 — Disk layer

Disk-utility-plan phase 0 (`ubix_disk_query` syscall + DTO + lib thunk) and
phase 0b (raw block read/write on both arches), plus GPT: read GPT in the
kernel's partition code alongside MBR, and a GPT writer (primary + backup
header, CRC32) in a userland library. Done when the kernel boots from a GPT
disk and a test writes, re-reads and validates a GPT on both arches.

### B6 — Disk tools on the device

`ubdisk` (partition: GPT create / add / delete), `format` (FAT32 ESP) and
`ubpool` (create the UbixFS pool) run inside uBixOS on top of B5. Done when the
three tools, run in-OS, turn a blank second virtio disk into the target
layout.

### B7 — Text installer

`libinstall` holds the steps (pick disk, partition, format, create pool, copy
the system, install the loader to the ESP, set root password / hostname / user,
verify) as a typed API with progress callbacks; `ubinstall` is the console
front end on it. Done when, on both arches, boot live media → `ubinstall` →
reboot → the installed disk boots to login, all under QEMU and in the B4 CI
job.

### B8 — Graphical installer

A `views` app on `libinstall`, started from the live desktop. No install logic
of its own. Done when it installs the same result as B7.

### B9 / B10 / B11 — Storage drivers

AHCI (SATA), NVMe and USB mass storage (bulk-only, SCSI transparent command
set over the existing USB stack), each from its public specification, each
registering with the B5 disk layer so the installer sees it. Done for each
when QEMU boots, partitions and installs to a disk on that controller, on
both arches where the controller exists there.

## Clean room

Every lane works from public specifications: UEFI, ACPI, GPT (in the UEFI
spec), AHCI, NVMe, USB MSC / SCSI, POSIX/SUS. No FreeBSD or Linux code; no GPL
code ever enters the tree. Reusing third-party code (e.g. headers from an
existing EFI toolkit) is a decision for the person, and anything taken goes
under `contrib/` or `tools/ports/` per
[third-party-ports-plan.md](third-party-ports-plan.md).

## Open choices for the person

Noted here; no lane waits on them. Each lane takes the default in brackets
unless the person says otherwise.

1. **Loader toolchain** — how the PE32+ loader is produced: clang/lld-link
   with a Windows/UEFI target, or ELF + a PE conversion step. [clang/lld]
2. **Secure Boot** — not signed; users disable Secure Boot. Shim/signing
   later? [no Secure Boot for now]
3. **Ramdisk format** — a UbixFS image (one filesystem everywhere) or a plain
   archive (ustar) unpacked into a memory filesystem. [UbixFS image]
4. **Install payload** — copy the running live root file by file, or write a
   prebuilt pool image and grow it. [prebuilt pool image, then grow]
5. **Media format** — raw GPT disk image (`.img`, works for VMs and `dd` to
   USB) or also a hybrid ISO for optical / VirtualBox CD boot. [`.img` first;
   ISO later]
6. **Swap** — keep a swap partition, or a swap file on the pool. [partition,
   as today]
7. **Pi / U-Boot** — the Pi keeps its MBR + U-Boot SD image, or moves to U-Boot's
   EFI path and the same media as QEMU `virt`. [keep U-Boot for now]
8. **Default sizes** — ESP and swap sizes on the installed disk. [ESP 256 MB,
   swap 1 GB or 5 % of the disk, whichever is less]
