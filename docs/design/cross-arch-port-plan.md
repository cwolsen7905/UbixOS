# UbixOS Cross-Architecture Port Plan

## Goal

Boot UbixOS on a second CPU architecture. The musl libc migration (Phases 1–6)
was designed with this in mind — all userland C and C++ code recompiles untouched
for a new arch. The remaining work is entirely in the kernel and the musl arch shim.

## Prerequisites

All met as of 2026-05-18:

- musl libc in tree (`contrib/musl/`) — arch shim is a single directory per target
- libc++ and libcxxabi in tree — pure C++, no arch assembly
- Build system auto-detects Darwin/FreeBSD and sets `CROSS_PREFIX` — extend for new target
- Linker script (`sys/compile/ldscript.i386`) parameterized by `cross-arch-plan.md` work

## Target Candidates

| Arch | QEMU machine | Toolchain (Homebrew) | Notes |
|------|-------------|----------------------|-------|
| x86_64 | `qemu-system-x86_64` | `x86_64-elf-gcc` (already installed) | 64-bit x86; syscall via `syscall` insn not `int $0x80`; easiest path |
| ARM (32-bit) | `qemu-system-arm -M virt` | `arm-none-eabi-gcc` | Thumb-2; no existing PCI/IDE driver |
| AArch64 | `qemu-system-aarch64 -M virt` | `aarch64-elf-gcc` | Clean arch; UEFI boot instead of BIOS/multiboot |
| RISC-V 64 | `qemu-system-riscv64 -M virt` | `riscv64-elf-gcc` | SBI firmware; OpenSBI as bootloader |

**Recommended first target: x86_64** — same ISA as i386, same QEMU device model,
same toolchain already installed, multiboot2 works, PCI/IDE/e1000 drivers reuse as-is.
Mostly a calling-convention and register-width exercise.

---

## Phase 1 — musl arch shim

**Files to add:**

| File | Contents |
|------|----------|
| `contrib/musl/arch/<newarch>/syscall_arch.h` | Inline `__syscall0`…`__syscall6` using the new arch's syscall instruction and registers |
| `contrib/musl/arch/<newarch>/bits/syscall.h.in` | Syscall number table — map POSIX names to UbixOS kernel slot numbers for the new ABI |
| `contrib/musl/arch/<newarch>/bits/alltypes.h.in` | Primitive types (`long`, pointer size, etc.) for the new arch |

For x86_64 the syscall instruction replaces `int $0x80`; rdi/rsi/rdx/r10/r8/r9 are the
argument registers; rax holds the number and return value.

**Done when:** `bmake world ARCH=x86_64` produces a statically-linked ELF binary that
passes `file` as `ELF 64-bit LSB executable, x86-64`.

---

## Phase 2 — Kernel entry point and exception vectors

**For x86_64:**

- Add `sys/init/start64.S` — long-mode entry after GRUB2 multiboot2 handoff; set up GDT64,
  TSS64, switch to 64-bit stack, call `kmain`
- Add `sys/arch/x86_64/` — IDT setup, syscall (`SYSCALL`/`SYSRET`) entry path,
  context switch (`switch_to`)
- Port linker script to `ldscript.x86_64` — higher-half kernel at `0xFFFFFFFF80000000`
  (standard x86_64 convention) or keep low like i386 for simplicity initially

**Done when:** QEMU reaches `kmain` and prints the boot banner over serial.

---

## Phase 3 — VMM and paging

x86_64 uses 4-level paging (PML4 → PDPT → PD → PT). The i386 VMM
(`sys/vmm/`) assumes 32-bit two-level paging and will need a parallel
implementation or parameterization.

- Add `sys/vmm/x86_64/paging64.c` — PML4 setup, `vmm_mapPage`, `vmm_getPhysicalAddr`
- Port `vmm_copyVirtualSpace` (fork COW logic) for 4-level tables
- MMIO guard rules from CLAUDE.md still apply — physical frames ≥ `numPages` are MMIO,
  skip `freePage` for them

**Done when:** Kernel can map its own text/data and handle a page fault.

---

## Phase 4 — Device drivers and boot to shell

With VMM working, the existing drivers need minor 64-bit cleanups:

- `sys/pci/` — PCI config space access is port I/O, works unchanged
- `sys/pci/e1000.c` — MMIO pointer widths need `uintptr_t` audit
- `sys/pci/ide.c` / `sys/fs/fat/` — sector I/O is 32-bit clean
- `sys/isa/atkbd.c`, `sys/isa/pit.c` — ISA port I/O, unchanged

**Done when:** UbixOS boots to a login prompt on x86_64 QEMU and `uname` reports the
new architecture.

---

## Kernel cleanup (post-port)

Once a second architecture exists, split i386-specific code out of generic files:

- Move `sys_set_thread_area` (LDT[1], `0xF` selector) from `sys/kernel/gen_calls.c`
  into `sys/arch/i386/tls.c` with a header at `sys/include/machine/tls.h`
- Repeat for any other i386-specific syscalls that have accumulated in `gen_calls.c`

---

## Status

Last updated: 2026-05-18

| Phase | Description | Status |
|-------|-------------|--------|
| Phase 1 | musl arch shim for new target | ⬜ Not started |
| Phase 2 | Kernel entry point and exception vectors | ⬜ Not started |
| Phase 3 | VMM and paging for new arch | ⬜ Not started |
| Phase 4 | Drivers and boot to shell | ⬜ Not started |
