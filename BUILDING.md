# Building UbixOS

This guide covers toolchain requirements, build steps, and the recommended VirtualBox VM workflow.

---

## Table of Contents

1. [Toolchain Requirements](#toolchain-requirements)
2. [Build Commands](#build-commands)
3. [Build Output](#build-output)
4. [VirtualBox VM Workflow](#virtualbox-vm-workflow)
5. [Installation](#installation)
6. [Troubleshooting](#troubleshooting)

---

## Toolchain Requirements

UbixOS compiles to bare-metal i386. You need a C/C++ cross toolchain targeting 32-bit x86:

| Tool | Minimum |
|------|---------|
| `cc` | GCC or Clang with `-m32` support |
| `c++` | G++ or Clang++ with `-m32` support |
| `as` | GNU Assembler (binutils) |
| `ld` | GNU Linker (binutils) |
| `ar`, `ranlib`, `nm`, `objcopy` | GNU binutils |
| `make` | BSD make (`bmake`) or GNU make |

The build assumes a BSD-hosted build environment (FreeBSD or a comparable system). A preconfigured **MrOlsen-BSD VM** (see below) is the recommended build host.

Key compiler flags used throughout:

```
-O -Wall -nostdlib -nostdinc -fno-builtin -fno-exceptions -ffreestanding
```

These flags disable host libc and standard includes, ensuring only UbixOS headers and libraries are used.

---

## Build Commands

Run all commands from the repository root (`/ubixos/usr/src` inside the build VM, or wherever you have cloned the repo).

```sh
# Build everything: kernel + world, then install
make

# Build the kernel only (sys/ subtree)
make kernel

# Build the userland only (lib/, libexec/, bin/)
make world

# Install kernel to /ubixos and /ubixos_fat
make install-kernel

# Install world to /ubixos and /ubixos_fat
make install-world

# Install both
make install

# Clean kernel objects
make clean-kernel

# Clean everything
make clean
```

After any install, run `sync` to flush filesystem buffers:

```sh
make; sync
```

---

## Build Output

Compiled objects land in `build/` at the repository root. The final kernel binary is produced at:

```
sys/compile/kernel
```

Userland binaries, libraries, and libexec components are staged to:

```
build/bin/
build/lib/
build/libexec/
```

`make install` copies these to the `/ubixos` (native UbixFS) and `/ubixos_fat` (FAT) mount points and then remounts `/ubixos_fat`.

---

## VirtualBox VM Workflow

The recommended development environment uses two VirtualBox VMs:

### MrOlsen-BSD VM (Build Host)

- Preconfigured BSD system with the full toolchain installed.
- SSH is enabled; the VM uses DHCP, so check the assigned IP after start.
- The UbixOS virtual disk (`UbixOS.vdi`) is attached and mounted at `/ubixos`.
- Start headless:

  ```sh
  VBoxHeadless -s MrOlsen-BSD
  ```

- SSH in and build:

  ```sh
  ssh <username>@<vm-ip>
  su                        # switch to root
  cd /ubixos/usr/src
  make; sync
  ```

### UbixOS VM (Target)

- Preconfigured to boot from the shared 2 GB virtual disk written by the build VM.
- RDP is enabled on port 3389.
- Start headless:

  ```sh
  VBoxHeadless -s UbixOS
  ```

- View the desktop via an RDP client connecting to `localhost:3389`.
- To reboot UbixOS after a kernel change: press **Alt+C** in the RDP session.

> **Note:** Only a kernel change requires a full VM reboot. Userland changes take effect on the next process launch without rebooting.

### Shared Disk

The 2 GB virtual disk is shared between both VMs. The build VM writes to it; the UbixOS VM reads and boots from it. Do not run both VMs with write access to the disk simultaneously.

---

## Installation

`make install` performs the following steps automatically:

1. Copies `build/bin/*` to `$ROOT/bin/` and `$ROOT_FAT/bin/`
2. Copies `build/lib/*` and `build/libexec/*` to both mount points
3. Copies `etc/*` to both mount points
4. Calls `sync` after each copy step
5. Unmounts and remounts `/ubixos_fat`

Default values:

```
ROOT     = /ubixos
ROOT_FAT = /ubixos_fat
```

Override on the command line if your mount points differ:

```sh
make install ROOT=/mnt/ubixos ROOT_FAT=/mnt/ubixos_fat
```

---

## Troubleshooting

**Build fails with "cannot find -lc" or similar linker errors**
Ensure the UbixOS libc is built before binaries: `make world` builds `lib/` before `bin/` automatically. Running `make kernel` followed immediately by `make install-kernel` skips world — that is fine for kernel-only changes.

**Kernel does not boot after install**
Verify `sync` ran after `make install`. If the FAT volume was not remounted cleanly, the bootloader may read stale data. Manually run `umount /ubixos_fat && mount /ubixos_fat`.

**Dynamic linker cannot find libraries at runtime**
The runtime linker expects libraries at `sys:/lib/`. Ensure `make install-world` completed successfully and that the `lib/` directory on the UbixOS volume is populated.

**Keyboard input erratic inside the VM**
This is a known issue with the AT keyboard driver (`sys/isa/atkbd.c`). Rebooting the UbixOS VM typically restores normal input.
