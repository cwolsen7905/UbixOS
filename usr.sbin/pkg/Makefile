# (C) 2002-2026 The UbixOS Project
# pkg — the on-device package client (installs to /usr/sbin).  Thin front end
# over lib/libpkg; see docs/design/packages-plan.md section 4.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = pkg
OBJS   = main.o util.o db.o add.o del.o query.o

# The OS series and base version packages are matched against, from the single
# source of truth (sys/include/ubixos/version.h).
_VERH   = ${SRCTOP}/sys/include/ubixos/version.h
_MAJOR != awk '/define UBIXOS_VERSION_MAJOR/ {print $$3}' ${_VERH}
_MINOR != awk '/define UBIXOS_VERSION_MINOR/ {print $$3}' ${_VERH}
_PATCH != awk '/define UBIXOS_VERSION_PATCH/ {print $$3}' ${_VERH}

EXTRA_CFLAGS = -I${SRCTOP}/contrib/zlib -DPKG_OS_ABI=${_MAJOR} -DPKG_OS_VERSION=\"${_MAJOR}.${_MINOR}.${_PATCH}\"
EXTRA_LIBS   = ${OBJ_DIR}/lib/libpkg.a -L${OBJ_DIR}/lib -lbearssl -lz

.include "${UBIX_MK}/ubix.musl.prog.mk"
