# bin/ps/Makefile — ps: list processes from /proc/<pid>/status.
#
# Standalone (not busybox): the busybox ps is Linux-/proc-layout specific and
# was never wired in; the kernel's procfs status format is what we parse.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = ps

OBJS = main.o

.include "${UBIX_MK}/ubix.musl.prog.mk"
