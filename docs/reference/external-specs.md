# External Specifications

Reference documents used during UbixOS development. These are not
distributed in the repository; download them from the sources below.

## ELF ABI

| Document | Source |
|----------|--------|
| ELF-64 Object File Format | https://www.uclibc.org/docs/elf-64-gen.pdf |
| System V ABI i386 supplement | https://www.sco.com/developers/devspecs/abi386-4.pdf |
| TIS ELF Specification v1.2 | https://refspecs.linuxbase.org/elf/elf.pdf |

The ELF type and flag constants used by the UbixOS ELF loader
(`sys/kernel/exec.c`) follow the i386 supplement.

## Intel Architecture

| Document | Source |
|----------|--------|
| Intel 64 and IA-32 Architectures SDM (combined volumes) | https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html |

Volume 3A covers paging, protection, task switching, and the TSS — all
directly relevant to `sys/vmm/` and `sys/arch/i386/`.

## Multiboot

| Document | Source |
|----------|--------|
| Multiboot Specification v0.96b | https://www.gnu.org/software/grub/manual/multiboot/multiboot.html |

UbixOS uses the Multiboot 1 protocol (`sys/init/start.S`).

## FAT File System

| Document | Source |
|----------|--------|
| Microsoft FAT32 File System Specification | https://academy.cba.mit.edu/classes/networking_communications/SD/FAT.pdf |

The FAT driver (`sys/fs/fat/`) implements FAT32 with LBA addressing.
