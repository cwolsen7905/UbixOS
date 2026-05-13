# Debugging UbixOS

---

## Serial Output

`kprintf` writes to both the VGA console and COM1 serial. When running under
QEMU, COM1 is captured to `serial.log`:

```sh
bmake run           # serial goes to serial.log
tail -f serial.log  # watch live in a second terminal
```

For headless runs with serial directly on the terminal:

```sh
bmake run-debug     # Ctrl-A X to exit
```

Clear the log before a fresh boot to avoid reading stale output:

```sh
rm -f serial.log && bmake run
```

---

## GDB

Start QEMU with the GDB stub enabled and halted at startup:

```sh
qemu-system-i386 -m 256 \
  -drive file=ubixos.img,format=raw,if=ide,index=0 \
  -serial file:serial.log -vga std \
  -device pcnet -net user \
  -s -S
```

Then connect from a second terminal:

```sh
x86_64-elf-gdb sys/compile/kernel
(gdb) target remote localhost:1234
(gdb) continue
```

The VS Code debug launch (`.vscode/launch.json`) automates this — use
**Debug UbixOS (QEMU)** (`Ctrl+Shift+D`).

For better source-level stepping, build the kernel without optimization:

```sh
# In sys/Makefile, change -O to -O0, then:
bmake kernel
```

### Useful GDB commands

```gdb
info registers          # dump all GPRs
x/10i $eip              # disassemble 10 instructions at EIP
x/4xw $esp              # inspect top of stack
info threads            # not meaningful (single task from GDB perspective)
b kmain                 # breakpoint at kernel C entry
b sys_execve            # breakpoint at execve syscall handler
```

---

## Debug Compile-Time Defines

Add these to the CFLAGS of the relevant subsystem Makefile (or to
`sys/Makefile.incl`) to enable verbose logging:

| Define | Effect |
|--------|--------|
| `DEBUG_EXEC` | Logs ELF loading, exec path selection, and segment placement |
| `DEBUG_VFS` | Logs VFS open/read/write/close dispatch and mount resolution |
| `DEBUG_SYSCTL` | Logs sysctl MIB lookups and value reads |

Example — enable VFS debug for one build:

```sh
bmake kernel CFLAGS="-DDEBUG_VFS"
```

Or add permanently to `sys/Makefile.incl`:

```makefile
CFLAGS += -DDEBUG_VFS
```

Remember to remove debug flags before committing.

---

## Kernel Panics

A kernel panic prints a message via `kprintf` then halts. Because the VGA
display may clear or freeze before you can read it, always check `serial.log`
after a crash — the panic message and register dump appear there.

Common panic signatures:

| Message | Likely cause |
|---------|-------------|
| `Page Fault @ 0x...` | NULL dereference, bad pointer, or missing VMM mapping |
| `#UD` / fault 6 | SSE/MMX instruction executed — missing `-mno-sse` flag in a Makefile |
| `Exec Format Error` | Binary not found on disk image, or image not rebuilt after `bmake world` |
| Triple fault (QEMU resets) | Stack overflow, corrupted GDT/IDT, or double fault handler itself faulting |

For triple faults, add `-d int` to the QEMU command to log all CPU exceptions:

```sh
qemu-system-i386 -d int -D qemu.log [other flags]
```

---

## QEMU Monitor

When QEMU is running in the VGA window, press **Ctrl-Alt-2** to enter the
QEMU monitor. Useful commands:

```
info registers     # CPU register state
info mem           # page table summary
xp /10xw 0x100000 # examine physical memory
q                  # quit QEMU
```

Press **Ctrl-Alt-1** to return to the guest display.
