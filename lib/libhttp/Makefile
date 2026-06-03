# (C) 2002-2026 The UbixOS Project
# libhttp — HTTP/1.0 + HTTPS client over lwIP sockets and BearSSL.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
BEARSSL  = ${SRCTOP}/contrib/bearssl

INCLUDES = -I${SRCTOP}/include \
           -I${BEARSSL}/inc \
           -I${MUSL_SRC}/include \
           -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 \
           -I${MUSL_SRC}/arch/generic

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC \
         -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP \
         -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libhttp
OBJS   = http.o

.PATH: ${.CURDIR}
.PATH.o: ${OBJDIR}

.SUFFIXES: .o .c

.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} \
		-c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libhttp.so

# Links libbearssl.so (TLS) and libc; both resolved from /lib at load time.
${OBJ_DIR}/lib/libhttp.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libhttp.so \
		${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lbearssl -lc \
		-o ${OBJ_DIR}/lib/libhttp.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libhttp.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
