# Building UbixOS

UbixOS targets bare-metal i386. There are two supported build hosts:

- **macOS** (Apple Silicon or Intel) — cross-compiles using Homebrew toolchain, runs under QEMU. This is the active development path on the `feature/macos-build-qemu` branch.
- **FreeBSD** — native build using the host toolchain, boots on real hardware or VirtualBox.

---

## Table of Contents

1. [macOS (QEMU)](#macos-qemu)
   - [Prerequisites](#prerequisites)
   - [Build](#build)
   - [Create Disk Image](#create-disk-image)
   - [Run in QEMU](#run-in-qemu)
   - [Serial Output](#serial-output)
   - [GDB Debugging](#gdb-debugging)
   - [macOS Troubleshooting](#macos-troubleshooting)
2. [FreeBSD / VirtualBox (Legacy)](#freebsd--virtualbox-legacy)
3. [Build Output Reference](#build-output-reference)
4. [Install Targets](#install-targets)

---

## macOS (QEMU)

### Prerequisites

Install everything via Homebrew:

```sh
brew install x86_64-elf-binutils x86_64-elf-gcc bmake qemu mtools i686-elf-grub
```

| Package | Purpose |
|---------|---------|
| `x86_64-elf-binutils` | Cross binutils (as, ld, ar, objcopy, …) targeting bare ELF |
| `x86_64-elf-gcc` | Cross GCC; used with `-m32` to produce i386 output |
| `bmake` | BSD make — required, GNU make will not work |
| `qemu` | x86 emulator (`qemu-system-i386`) |
| `mtools` | FAT image tools (`mformat`, `mcopy`, `mmd`) used by `mkimage.sh` |
| `i686-elf-grub` | GRUB2 bootloader for embedding into the disk image |

> **Why `x86_64-elf-gcc -m32` and not `i386-elf-gcc`?**  
> The Homebrew `i386-elf-gcc` formula is unmaintained. `x86_64-elf-gcc` supports `-m32` and produces identical i386 output. The Makefile sets this up automatically when it detects Darwin.

> **Apple Silicon vs Intel path:**  
> Homebrew installs to `/opt/homebrew` on Apple Silicon and `/usr/local` on Intel. `mkimage.sh` hardcodes the GRUB library path to `/opt/homebrew/…`. If you are on an Intel Mac, edit the `GRUB_LIB` variable near the top of `tools/mkimage.sh`.

### AArch64 cross-toolchain (for `TARGET=aarch64`)

Building the arm64 port additionally needs an `aarch64-elf-` cross GCC + binutils:

```sh
brew install aarch64-elf-gcc aarch64-elf-binutils
```

The Makefile defaults `CROSS_PREFIX=aarch64-elf-` for `TARGET=aarch64`, so once
these are on `PATH` no override is needed: `bmake kernel world TARGET=aarch64`.

> **Pre-release macOS (Homebrew `:dunno` / "no bottle available"):**  
> On a macOS version newer than your Homebrew knows about (e.g. a beta), the
> `aarch64-elf-*` formulae fail two ways: plain install reports `no bottle
> available`, and `--build-from-source` aborts with
> `unknown or unsupported macOS version: :dunno` because Homebrew can't classify
> the OS. Two workarounds:
>
> 1. **Fake the version for the build** — set `HOMEBREW_FAKE_MACOS` to the
>    highest release your Homebrew supports (check
>    `brew ruby -e 'puts MacOSVersion::SYMBOLS'`), e.g. on a box where Tahoe (26)
>    is the newest known:
>    ```sh
>    HOMEBREW_FAKE_MACOS=26 brew install aarch64-elf-gcc aarch64-elf-binutils
>    ```
> 2. **Use the prebuilt Arm cask instead** (a signed `.pkg`, no formula version
>    gate — needs your password for the installer):
>    ```sh
>    brew install --cask gcc-aarch64-embedded
>    ```
>    This installs the `aarch64-none-elf-` prefix under
>    `/Applications/ArmGNUToolchain/<ver>/aarch64-none-elf/bin`. Either add that
>    dir to `PATH` and build with `CROSS_PREFIX=aarch64-none-elf-`, or symlink the
>    tools to the repo-default `aarch64-elf-` prefix in `/opt/homebrew/bin`.

---

### Build

All commands use `bmake` from the repository root. GNU `make` will not work — the Makefiles use BSD-specific syntax.

```sh
# Build everything: kernel + world
bmake

# Kernel only (faster when only sys/ changed)
bmake kernel

# Userland only (lib/, libexec/, bin/)
bmake world

# Clean everything
bmake clean
```

The Makefile detects Darwin automatically and sets `CROSS_PREFIX=x86_64-elf-` and `CROSS_M32=-m32`. No manual configuration is needed.

---

### Create Disk Image

```sh
bmake image
```

This calls `tools/mkimage.sh` which produces `ubixos.img` — a 256 MB raw disk image containing:

- **Sectors 0–0**: GRUB `boot.img` (MBR)
- **Sectors 1–2047**: GRUB `core.img` (stage 1.5, loads FAT driver and `grub.cfg`)
- **LBA 2048+**: FAT32 partition with:
  - `boot/grub/grub.cfg` and GRUB modules
  - `boot/kernel/kernel` — the UbixOS kernel
  - `bin/`, `lib/`, `libexec/` — userland
  - `etc/userdb`, `etc/fstab`, `etc/motd`

To update only the kernel in an existing image (faster than a full rebuild):

```sh
bmake kernel-to-image
```

---

### Run in QEMU

```sh
bmake run
```

This launches:

```sh
qemu-system-i386 -m 256 \
  -drive file=ubixos.img,format=raw,if=ide,index=0 \
  -serial file:serial.log \
  -vga std \
  -device pcnet -net user
```

QEMU opens a VGA window. Serial output from `kprintf` goes to `serial.log` rather than the window.

For a headless run with serial output directly on the terminal (useful for CI or quick checks — press `Ctrl-A X` to exit):

```sh
bmake run-debug
```

---

### Serial Output

Kernel debug output (`kprintf`) is sent to both the VGA console and COM1. When using `bmake run`, COM1 is captured to `serial.log`:

```sh
# Watch it live in a second terminal
tail -f serial.log
```

`serial.log` persists across runs and accumulates. Clear it before a fresh boot if you want a clean log:

```sh
rm -f serial.log && bmake run
```

---

### Login

The system boots to a login prompt. Default credentials (from `tools/userdb`):

```
login: root
password: user
```

---

### GDB Debugging

Add `-s -S` to the QEMU command to enable the GDB stub and halt at startup:

```sh
qemu-system-i386 -m 256 \
  -drive file=ubixos.img,format=raw,if=ide,index=0 \
  -serial file:serial.log -vga std \
  -device pcnet -net user \
  -s -S
```

Then in a second terminal, connect with the cross GDB:

```sh
x86_64-elf-gdb sys/compile/kernel
(gdb) target remote localhost:1234
(gdb) continue
```

The VS Code launch configuration (`.vscode/launch.json`) automates this — use the **Debug UbixOS (QEMU)** launch target with `Ctrl+Shift+D`.

> **Note:** GDB source stepping works best if the kernel was built without `-O` (optimization). Edit `sys/Makefile` and change `-O` to `-O0` for a debug build, then `bmake kernel`.

---

### macOS Troubleshooting

**`bmake: command not found`**  
Install with `brew install bmake`. If Homebrew's bin directory is not in your PATH, add it:  
`export PATH="/opt/homebrew/bin:$PATH"` (Apple Silicon) or `export PATH="/usr/local/bin:$PATH"` (Intel).

**`x86_64-elf-gcc: command not found` during build**  
Install with `brew install x86_64-elf-gcc`. The Makefile sets `CROSS_PREFIX=x86_64-elf-` automatically on Darwin — do not set it manually unless you are overriding.

**`ERROR: GRUB boot.img not found at /opt/homebrew/Cellar/i686-elf-grub/…`**  
The `GRUB_LIB` path in `tools/mkimage.sh` is version-specific. Check the actual installed path:  
```sh
find /opt/homebrew/Cellar/i686-elf-grub -name boot.img
```  
Update the `GRUB_LIB` variable at the top of `tools/mkimage.sh` to match.

**`mformat: command not found`**  
Install with `brew install mtools`.

**Build succeeds but QEMU crashes immediately**  
Check `serial.log` — the kernel prints its boot sequence over serial before anything appears on the VGA display. A very early crash (before `kmain`) will show nothing; check that `bmake image` completed without errors and that `sys/compile/kernel` exists and is non-zero in size.

**`#UD` fault / kernel triple-faults after adding code**  
GCC is emitting SSE/MMX instructions (movdqa, etc.) for struct copies or memset. All kernel and world code must be compiled with `-mno-sse -mno-sse2 -mno-mmx -mno-3dnow`. Check that the relevant Makefile includes these flags. The kernel never sets `CR4.OSFXSR`, so any XMM instruction triggers fault 6.

**Dynamic linker fails to find libraries**  
The runtime linker (`libexec/ld`) looks for libraries at `sys:/lib/`. Ensure `bmake world && bmake image` ran successfully and that `build/lib/` is populated before creating the image.

---

## FreeBSD / VirtualBox (Legacy)

This was the original development workflow. The `master` branch targets this environment.

### Toolchain

FreeBSD provides a native GCC/binutils that targets i386 directly. No cross-compiler prefix is needed — `Makefile.incl` sets `CROSS_PREFIX=` (empty) on non-Darwin hosts.

```sh
# FreeBSD: use plain make (which is bmake)
make
make kernel
make world
```

### VirtualBox VM Workflow

The recommended setup uses two VMs:

#### MrOlsen-BSD (Build Host)

- Preconfigured FreeBSD system with the full toolchain.
- The UbixOS virtual disk (`UbixOS.vdi`) is attached and mounted at `/ubixos` and `/ubixos_fat`.
- Start headless:

  ```sh
  VBoxHeadless -s MrOlsen-BSD
  ```

- SSH in and build:

  ```sh
  ssh <username>@<vm-ip>
  su
  cd /ubixos/usr/src
  make; sync
  ```

#### UbixOS (Target VM)

- Boots from the shared virtual disk written by the build VM.
- RDP is enabled on port 3389.
- Start headless:

  ```sh
  VBoxHeadless -s UbixOS
  ```

- Connect via RDP to `localhost:3389`.
- Reboot after a kernel change: press **Alt+C** in the RDP session.

> Only kernel changes require a VM reboot. Userland changes take effect on the next process launch.

### Installation (FreeBSD)

```sh
make install
```

Copies build artifacts to the mount points:

```
ROOT     = /ubixos       (UFS)
ROOT_FAT = /ubixos_fat   (FAT copy)
```

Override on the command line if your mount points differ:

```sh
make install ROOT=/mnt/ubixos ROOT_FAT=/mnt/ubixos_fat
```

---

## Build Output Reference

| Path | Contents |
|------|----------|
| `sys/compile/kernel` | Final kernel ELF binary |
| `build/bin/` | Userland executables |
| `build/lib/` | Shared and static libraries |
| `build/libexec/` | Runtime dynamic linker (`ld.so`) |
| `ubixos.img` | Bootable QEMU disk image (macOS path) |
| `serial.log` | QEMU COM1 capture (created by `bmake run`) |

---

## Install Targets

| Target | Action |
|--------|--------|
| `bmake` | Build kernel + world |
| `bmake kernel` | Build kernel only |
| `bmake world` | Build userland only |
| `bmake image` | Create `ubixos.img` from build output |
| `bmake run` | Launch QEMU with `ubixos.img` |
| `bmake run-debug` | Launch QEMU headless, serial to stdout |
| `bmake kernel-to-image` | Hot-patch kernel into existing image via `mcopy` |
| `bmake install` | Install to `/ubixos` + `/ubixos_fat` (FreeBSD) |
| `bmake clean` | Remove all build artifacts |
