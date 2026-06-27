# tests — in-OS test/dev harnesses, installed to /usr/tests
# (C) 2002-2026 The UbixOS Project
#
# Not shipped applications: syscall/fork/thread/fs/net/tty exercisers and bring-up
# demos.  FreeBSD homes its equivalents under /usr/tests.  Add a directory name
# to SUBDIRS to include it in the build and clean targets.
#
# The common set builds on both 64-bit arches; the asm/thread/crypto exercisers
# below build only on i386 (frozen on releng/2) — they were never wired into the
# 64-bit world, so they stay i386-gated here to preserve that.
SUBDIRS = syscheck filetest pipetest selfhostck ttytest udptest fbtest
.if ${_ARCH} == "i386"
SUBDIRS += clonetest tlstest futextest pthreadtest condtest detachtest vmtest \
           nsfbtest rndtest pwtest ssltest hello
.endif

.PHONY: all clean ${SUBDIRS}

.for _dir in ${SUBDIRS}
${_dir}:
	(cd ${_dir};${MAKE})
.endfor

all: ${SUBDIRS}

clean:
.for _dir in ${SUBDIRS}
	(cd ${_dir};${MAKE} clean)
.endfor
