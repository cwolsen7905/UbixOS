# UbixOS Documentation

## Architecture

| Document | Description |
|----------|-------------|
| [architecture/vmm.md](architecture/vmm.md) | Virtual Memory Manager: layout, functions, page-fault handling |
| [architecture/task-switching.md](architecture/task-switching.md) | Task switching internals: TSS, GDT, scheduler, fork, FPU |

## Design Specs

| Document | Description |
|----------|-------------|
| [design/fbcon.md](design/fbcon.md) | VESA framebuffer console spec (draft) |

## Driver Development

| Document | Description |
|----------|-------------|
| [drivers/writing-a-driver.md](drivers/writing-a-driver.md) | How to write a UbixOS device driver |

## Root-Level Reference

| Document | Description |
|----------|-------------|
| [../README.md](../README.md) | Project overview and quick start |
| [../BUILDING.md](../BUILDING.md) | Build instructions (macOS and FreeBSD) |
| [../ARCHITECTURE.md](../ARCHITECTURE.md) | High-level source tree architecture |
| [../SYSCALLS.md](../SYSCALLS.md) | Syscall tables and ABI reference |
| [../DEBUG.md](../DEBUG.md) | Debugging under QEMU / GDB |
| [../BUGS.md](../BUGS.md) | Known bugs and workarounds |
| [../TODO.md](../TODO.md) | Work items and future plans |
| [../CHANGELOG.md](../CHANGELOG.md) | Release history |

## Legacy Reference Files

| File | Notes |
|------|-------|
| `doc/ELF_Format.pdf` | Intel ELF ABI specification (external reference) |
| `doc/vmm/i386_vmm_page_reference.xlsx` | Page-directory index reference spreadsheet |
| `doc/UbixOS_Build.txt` | Historical VirtualBox workflow (superseded by BUILDING.md) |
| `doc/sample_driver.c` | Original C driver skeleton (see drivers/writing-a-driver.md) |
