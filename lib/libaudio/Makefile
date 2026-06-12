# (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OBJDIR  = ${OBJ_DIR}/obj/libaudio

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl

# Per-arch musl header dir + ISA flags (i386 needs -m32 and the no-SSE knobs the
# kernel's missing CR4.OSFXSR demands; aarch64 needs neither).
.if ${_ARCH} == "aarch64"
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/aarch64
ARCH_CFLAGS   =
.else
ARCH_MUSL_INC = -I${MUSL_SRC}/arch/i386
ARCH_CFLAGS   = ${CROSS_M32} -mno-sse -mno-sse2 -mno-mmx -mno-3dnow
.endif

MUSL_INC = -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           ${ARCH_MUSL_INC} \
           -I${MUSL_SRC}/arch/generic \
           -I${SRCTOP}/include

# -fPIC: libaudio.a is only ever linked into PIE executables, and the veneer
# references an external libc data symbol (program_invocation_short_name) for the
# app's mixer name — GOT-indirect access (PIC) is required so the aarch64 PIE link
# does not emit an unresolvable ADR_PREL relocation against it.
MUSL_CFLAGS = ${ARCH_CFLAGS} -nostdlib -nostdinc -fno-builtin -fPIC \
              -MMD -MP -Wall -O

OBJS = audio.o

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(MUSL_CFLAGS) $(MUSL_INC) -MT ${.TARGET:T} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libaudio.a

${OBJ_DIR}/lib/libaudio.a: $(OBJS)
	$(AR) rcs ${OBJ_DIR}/lib/libaudio.a ${OBJS:S|^|${OBJDIR}/|}

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libaudio.a

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
