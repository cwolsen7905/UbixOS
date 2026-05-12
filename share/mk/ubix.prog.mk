# (C) 2002-2026 The UbixOS Project
# ubix.prog.mk — Compile and link rules for UbixOS userland programs.
#
# Include this at the END of a program Makefile, after BINARY and OBJS are defined.
# Variables supplied by bin/Makefile.incl: CC, CXX, CFLAGS, LDFLAGS, INCLUDES,
#                                          BUILD_DIR, STARTUP, LIBRARIES,
#                                          CROSS_PREFIX, REMOVE, OBJ_DIR.
#
# Required variables set before .include:
#   BINARY    — output binary name
#   OBJS      — list of .o files to compile
#
# Optional variables:
#   LINK_TYPE     — "static" (CC driver, default) or "dynamic" (ld direct)
#   EXTRA_LDFLAGS — appended to the link command (e.g. -static for init)
#   POST_LINK     — shell command run after linking (e.g. strip the binary)
#
# Object files are written to OBJDIR so the source tree stays clean.

LINK_TYPE     ?= static
EXTRA_LDFLAGS ?=
POST_LINK     ?= @true

OBJDIR ?= ${OBJ_DIR}/obj/bin/${.CURDIR:T}

.PATH.o: ${OBJDIR}

.SUFFIXES: .o .s .S .c .cc .C .cpp

.cc.o .C.o .cpp.o:
	@mkdir -p ${OBJDIR}
	$(CXX) -Wall -O $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) -Wall -O $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

.S.o:
	@mkdir -p ${OBJDIR}
	$(CC) -Wall $(CFLAGS) $(INCLUDES) -c -o ${OBJDIR}/${.TARGET} ${.IMPSRC}

_OBJS_FULL = ${OBJS:S|^|${OBJDIR}/|}

.if ${LINK_TYPE} == "dynamic"
$(BINARY): $(OBJS)
	${CROSS_PREFIX}ld -m elf_i386 -e _start -dynamic-linker sys:/libexec/ld.so \
		-o ${BUILD_DIR}/bin/${BINARY} $(STARTUP) ${_OBJS_FULL} $(LIBRARIES)
	${POST_LINK}
.else
$(BINARY): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(EXTRA_LDFLAGS) \
		-o $(BUILD_DIR)/bin/$(BINARY) $(STARTUP) $(LIBRARIES) ${_OBJS_FULL}
	${POST_LINK}
.endif

all: $(BINARY)

clean:
	$(REMOVE) ${OBJDIR} $(BUILD_DIR)/bin/$(BINARY)

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
