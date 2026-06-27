# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY        = automountd
# fsmount.o = the FreeBSD mount()/unmount() syscall wrappers (slots 21/22), local to
# automountd (its only caller).  They are NOT in libubix_api.so: their names collide
# with musl libc's mount(), and an exe-local definition cleanly shadows libc's.
OBJS          = main.o fsmount.o
.include "${UBIX_MK}/ubix.musl.ubix.prog.mk"
