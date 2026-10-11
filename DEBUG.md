# Debugging uBixOS

master is 64-bit only: **aarch64** (QEMU `virt`, the default) and **x86_64**
(QEMU PC, the second supported arch). The 32-bit i386 recipes are kept at the end
as [legacy history](#legacy-i386-releng2-only) — they do not work on master.

Build output is arch-homed: the kernel ELF (with symbols) is
`build/<arch>/boot/kernel`, and the disk images are `ubixos-arm.img` (aarch64) and
`ubixos-x86_64.img` (x86_64) in the repository root. The paths and QEMU flags below
come from the top-level `Makefile` (`run-*` targets); re-read those targets if they
look out of date.

---

## Ground rules: never drive someone else's QEMU or image

The `bmake run-*` targets attach the **real** disk image (`ubixos-arm.img`,
`ubixos-x86_64.img`) read-write, some of them with `sudo` and a bridged `vmnet`
interface. A debugging session that crashes the guest can corrupt that image's
filesystem.

- Do not start, signal, attach to or kill a QEMU you did not start yourself, and do not
  use a person's running instance's gdb port or monitor socket.
- For anything destructive or experimental, **copy the image first** and point QEMU at
  the copy (the recipes below do this). A human decides when the copy is thrown away.
- Agents and scripts should not run the graphical `run-*` targets (they need `sudo`,
  vmnet, a display and audio). Prefer the headless recipes, on a copied image.

---

## Serial output

`kprintf` goes to the serial console (the PL011 on aarch64, COM1 on x86_64) in
addition to any display console. Serial is the primary debugging channel — a
panic or fault dump may never be visible on the framebuffer.

### aarch64

```sh
bmake run-aarch64          # graphical; serial -> ./serial.log (HVF on Apple Silicon)
tail -f serial.log         # watch it live from a second terminal
bmake run-debug-aarch64    # headless; serial on the terminal, Ctrl-A X quits
```

Clear the log before a fresh boot so you are not reading stale output:

```sh
rm -f serial.log && bmake run-aarch64
```

### x86_64

```sh
bmake run-x86_64 TARGET=x86_64        # std-VGA window; serial multiplexed on stdio
bmake run-debug-x86_64 TARGET=x86_64  # headless; serial on the terminal, Ctrl-A X quits
```

`run-x86_64` does **not** write `serial.log`; its serial goes to the terminal that
started QEMU (`-serial mon:stdio`, `Ctrl-A C` toggles the monitor). Redirect it
yourself if you want a log (see the scratch-image recipe below). On Apple Silicon this
runs under TCG, so it is slow.

---

## Scratch-image recipes (copy first)

These start QEMU by hand on a **copy** of the image, headless, with user-mode
networking (no `sudo`, no vmnet). Build first: `bmake kernel world image` (and
`TARGET=x86_64` for the second arch). `SMP` below is the `bmake` default of 2.

### aarch64, HVF

```sh
cp ubixos-arm.img /tmp/ubixos-debug-arm.img
qemu-system-aarch64 -machine virt,gic-version=2 -accel hvf -cpu host -m 512 -smp 2 \
  -kernel build/aarch64/boot/kernel \
  -global virtio-mmio.force-legacy=false \
  -drive file=/tmp/ubixos-debug-arm.img,format=raw,if=none,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -device virtio-net-device,netdev=net0 -netdev user,id=net0 \
  -display none -serial file:/tmp/ubixos-debug-serial.log
```

`-global virtio-mmio.force-legacy=false` selects the modern virtio-mmio transport the
drivers need. To see the desktop, add `-device virtio-gpu-device -device
virtio-keyboard-device -device virtio-mouse-device` and drop `-display none`.

### aarch64, TCG (when HVF reports `Assertion failed: (isv)`)

Under HVF, a store through a bad pointer that lands on unmapped/device space dies inside
QEMU with `Assertion failed: (isv)` and no guest register state. That is a real
bad-pointer store in the guest, not a QEMU bug. Re-run the same recipe under TCG, which
delivers the abort to the guest's own fault path so the kernel logs the faulting
process, `pc` and `FAR`:

```sh
cp ubixos-arm.img /tmp/ubixos-debug-arm.img
qemu-system-aarch64 -machine virt,gic-version=2 -accel tcg -cpu max -m 512 -smp 2 \
  -kernel build/aarch64/boot/kernel \
  -global virtio-mmio.force-legacy=false \
  -drive file=/tmp/ubixos-debug-arm.img,format=raw,if=none,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -device virtio-net-device,netdev=net0 -netdev user,id=net0 \
  -display none -serial file:/tmp/ubixos-debug-serial.log
```

`-cpu max` is required with TCG (`-cpu host` means HVF passthrough). TCG is much slower.
The repository also has `bmake run-tcg-aarch64` (graphical, real image, `sudo`,
bridged networking) — use the copy recipe above unless you are the person who owns the
image.

### x86_64

```sh
cp ubixos-x86_64.img /tmp/ubixos-debug-x86_64.img
qemu-system-x86_64 -m 256 -smp 2 -cpu qemu64,+x2apic \
  -kernel build/x86_64/boot/kernel -nographic \
  -drive file=/tmp/ubixos-debug-x86_64.img,format=raw,if=none,id=hd0 \
  -device virtio-blk-pci,drive=hd0,disable-modern=true \
  -netdev user,id=n0 -device virtio-net-pci,netdev=n0,disable-modern=true
```

`-nographic` puts COM1 on the terminal; append `-serial file:/tmp/x.log` instead of
`-nographic` (plus `-display none`) to log to a file. The `disable-modern=true` flags
select the legacy I/O-BAR virtio the bring-up drivers speak.

---

## GDB

Add the gdb stub and halt at the first instruction by appending `-s -S` to any recipe
above (`-s` = `-gdb tcp::1234`; `-S` = stop at start). QEMU then waits for a debugger.
Use a scratch image, and a **free port** if another QEMU is running (`-gdb tcp::1235`
instead of `-s`).

Connect from a second terminal with a gdb that understands the target (for example
`aarch64-elf-gdb` / `x86_64-elf-gdb` from Homebrew, or `gdb-multiarch`; **TODO: verify**
the package names — `BUILDING.md` lists the compilers, not gdb) and load the **kernel
ELF** for symbols:

```sh
# aarch64
aarch64-elf-gdb build/aarch64/boot/kernel
(gdb) target remote localhost:1234
(gdb) b kmain_aarch64
(gdb) continue

# x86_64
x86_64-elf-gdb build/x86_64/boot/kernel
(gdb) target remote localhost:1234
(gdb) b kmain_x86_64
(gdb) continue
```

Notes that come from the linker scripts:

- **aarch64** is linked to run at high VAs (`0xFFFFFF8000000000 + KERNEL_PHYS`) but
  loaded at `KERNEL_PHYS`. The very first instructions (`.text.boot`, MMU off) execute at
  *physical* addresses, so a breakpoint on a high-VA symbol will not hit until the MMU is
  up; `kmain_aarch64` is past that point. **TODO: verify** how gdb resolves the low boot
  stub symbols on your toolchain.
- **x86_64** enters through the PVH note (32-bit entry) and then moves to long mode and
  the higher half (`KERNBASE = 0xFFFFFFFF80000000`). Break on `kmain_x86_64` or later for
  64-bit symbol-level stepping.
- Under **HVF**, gdb breakpoints and single-stepping may be limited; use
  `-accel tcg` for interactive debugging. **TODO: verify.**
- Build without optimisation for source-level stepping. The kernel flags are
  `AARCH64_KCFLAGS` / `X86_64_KCFLAGS` in the top-level `Makefile` (both currently `-O`);
  there is no `-O0` switch, so change the flag locally and remember to revert it.

### Useful gdb commands

```gdb
info registers            # all general registers
bt                        # backtrace (kernel frames)
x/10i $pc                 # disassemble at the program counter  (x86_64: x/10i $rip)
x/4gx $sp                 # top of stack (64-bit words)         (x86_64: x/4gx $rsp)
b sys_execve              # break on a syscall handler
b aarch64_syscall         # aarch64 syscall dispatcher (EL0 svc)
b x86_64_syscall          # x86_64 syscall dispatcher
b vmm_demand_fault        # demand-paging resolver (both arches)
```

---

## Debug compile-time defines

The 64-bit kernel is built from the top-level `Makefile` (not `sys/Makefile.incl`),
which already passes `-DDEBUG_SYSCTL` on aarch64.

| Define | Where it is read | Effect |
|--------|------------------|--------|
| `DEBUG_SYSCTL` | `sys/kern/kern_sysctl.c` | Logs sysctl MIB lookups (already on in `AARCH64_KCFLAGS`) |
| `DEBUG_VFS` | `sys/fs/vfs/file.c` | Logs VFS open/read/write/close dispatch |
| `DEBUG_VFS_CALLS` | `sys/posix/vfs_calls.c` | Logs the POSIX VFS syscall layer |

The old docs listed a `DEBUG_EXEC` define; no source file reads it on master, so it has
been dropped here.

To enable one for a single aarch64 build, use the existing hook:

```sh
bmake kernel AARCH64_KCFLAGS_EXTRA="-DDEBUG_VFS"
```

x86_64 has no equivalent `…_KCFLAGS_EXTRA` variable (`X86_64_KCFLAGS` in the top-level
`Makefile` is assembled in one place), so edit it locally for a one-off build. Remember to
remove debug flags before committing.

---

## Kernel panics and faults

Always read the serial log after a crash. Common signatures:

| Output | Arch | Likely cause |
|--------|------|--------------|
| `*** aarch64 synchronous exception ***` with `ESR_EL1`, `ELR_EL1`, `FAR_EL1` | aarch64 | An unhandled EL1 fault. `EC = ESR >> 26`: `0x25` data abort (same EL), `0x21` instruction abort, `0x00` unknown/undefined. `FAR_EL1` is the faulting address; `ELR_EL1` the faulting PC — look it up with `addr2line` or `x/i` in gdb |
| `EL0 fault: pid=… SIGSEGV at 0x… pc=0x…` | aarch64 | A user process touched an unmapped address; the process is killed with `SIGSEGV` |
| `*** aarch64 unexpected/invalid vector ***` | aarch64 | An exception arrived on a vector slot the kernel does not use (SP0 / AArch32) |
| `Assertion failed: (isv)` printed by QEMU itself | aarch64 + HVF | Bad-pointer store in the guest; re-run under TCG (see above) |
| `*** x86_64 exception: <name> (vec=N err=…)` with `RIP`, `CR2` | x86_64 | An unhandled exception. `vec=14` is a page fault: `CR2` is the address, `err` bit 0 = present, bit 1 = write, bit 2 = user |
| Exception `vec=6` (invalid opcode) | x86_64 | SSE/MMX/AVX in kernel code — the kernel is built `-mno-sse -mno-mmx`; a flag was dropped or an FP type crept in |
| `Exec Format Error` / exec failures | both | Binary missing from the image, or the image is stale: `bmake world image` |
| QEMU resets / loops with no output (triple fault) | x86_64 | Stack overflow, corrupt IDT/GDT, or a fault inside the fault handler; see below |

For hard x86_64 resets, log CPU exceptions and keep QEMU from rebooting:

```sh
# append to the x86_64 recipe above
-d int,cpu_reset -D /tmp/qemu.log -no-reboot
```

For aarch64, `-d int,guest_errors -D /tmp/qemu.log` logs exception entry and
invalid guest accesses (it slows the run noticeably; **TODO: verify** the output volume
under HVF — `-d` is a TCG feature and has no effect under HVF).

---

## QEMU monitor

With `-nographic` (or `-serial mon:stdio`), press **Ctrl-A C** to switch between the
serial console and the QEMU monitor; **Ctrl-A X** quits. In a graphical window use
**Ctrl-Alt-2** for the monitor and **Ctrl-Alt-1** to return to the guest. Useful
commands:

```
info registers      # CPU register state
xp /10xw 0x40200000 # examine physical memory (adjust the address)
info mtree          # memory map (RAM / devices)
q                   # quit QEMU
```

(`info mem` and `info tlb` are x86 monitor commands and are not available on aarch64.)

---

## Legacy: i386 (`releng/2` only)

> **Historical.** The 32-bit i386 port is frozen on the `releng/2` branch. master has no
> i386 kernel, no `qemu-system-i386` run target and no `sys/compile/kernel` ELF at that
> path. The block below is kept for people working on `releng/2`.

```sh
qemu-system-i386 -m 256 \
  -drive file=ubixos.img,format=raw,if=ide,index=0 \
  -serial file:serial.log -vga std \
  -device pcnet -net user \
  -s -S
```

Then, from a second terminal (on `releng/2`):

```sh
x86_64-elf-gdb sys/compile/kernel
(gdb) target remote localhost:1234
(gdb) continue
```

On i386 the registers are `$eip`/`$esp` (`x/10i $eip`, `x/4xw $esp`). The VS Code launch
configuration in `.vscode/launch.json` is the i386 one (`i386-elf-gdb`,
`sys/compile/kernel`, `set architecture i386`) and does **not** work against master's
64-bit kernels. Triple faults on i386 were chased with `qemu-system-i386 -d int -D
qemu.log`.
