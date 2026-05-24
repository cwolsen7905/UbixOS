# SDE (Software Display Environment) — Retired

The kernel-side SDE (`sys/sde/`) was removed in May 2026. It was a C++ kernel
thread (using objgfx40) that owned the VESA framebuffer directly. Its
responsibilities have been replaced by the userland display stack.

**See [display.md](display.md) for the current architecture.**

## What replaced what

| Old (SDE) | New |
|-----------|-----|
| `sdeThread` kernel thread | `bin/views` compositor process |
| `ogDisplay_UbixOS` kernel class | `ogSurface::ogAttach()` in userland |
| `sysSDE` syscall (slot 40) | retired (slot is now `sys_invalid`) |
| `ogPrintf` kernel text output | removed; use `kprintf` for serial debug |
| `sys/sde/` build directory | no longer compiled into the kernel |
| `lib/objgfx40/` kernel copy | `lib/objgfx/` userland-only library |
