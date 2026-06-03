# (C) 2002-2026 The uBixOS Project
# libhubbub — NetSurf HTML5 parser (vendored, MIT). entities.inc pre-generated
# on the build host (build/make-entities.pl) and vendored.

include ../../Makefile.incl
include ../Makefile.incl

MUSL_SRC = ${SRCTOP}/contrib/musl
MUSL_OBJ = ${OBJ_DIR}/obj/musl
HUB      = ${SRCTOP}/contrib/libhubbub
LPU      = ${SRCTOP}/contrib/libparserutils

INCLUDES = -I${HUB}/include -I${HUB}/src -I${LPU}/include \
           -I${MUSL_SRC}/include -I${MUSL_OBJ}/obj/include \
           -I${MUSL_SRC}/arch/i386 -I${MUSL_SRC}/arch/generic

CFLAGS = ${CROSS_M32} -nostdinc -fno-builtin -fPIC -fcommon \
         -mno-sse -mno-sse2 -mno-mmx -mno-3dnow -MMD -MP -Wall -O2

OBJDIR = ${OBJ_DIR}/obj/lib/libhubbub
OBJS   = after_after_body.o after_after_frameset.o after_body.o after_frameset.o after_head.o before_head.o before_html.o detect.o element-type.o entities.o errors.o generic_rcdata.o in_body.o in_caption.o in_cell.o in_column_group.o in_foreign_content.o in_frameset.o in_head_noscript.o in_head.o in_row.o in_select_in_table.o in_select.o in_table_body.o in_table.o initial.o parser.o string.o tokeniser.o treebuilder.o 

.PATH: ${HUB}/src ${HUB}/src/tokeniser ${HUB}/src/treebuilder ${HUB}/src/utils ${HUB}/src/charset
.PATH.o: ${OBJDIR}
.SUFFIXES: .o .c
.c.o:
	@mkdir -p ${OBJDIR}
	$(CC) $(CFLAGS) $(INCLUDES) -MT ${.TARGET:T} -MF ${OBJDIR}/${.TARGET:T:.o=.d} -c -o ${OBJDIR}/${.TARGET:T} ${.IMPSRC}

all: ${OBJ_DIR}/lib/libhubbub.so
${OBJ_DIR}/lib/libhubbub.so: $(OBJS)
	@mkdir -p ${OBJ_DIR}/lib
	$(LD) -m elf_i386 -shared -soname libhubbub.so ${OBJS:S|^|${OBJDIR}/|} \
		-L${OBJ_DIR}/lib -lparserutils -lc -o ${OBJ_DIR}/lib/libhubbub.so

clean:
	$(REMOVE) ${OBJDIR} ${OBJ_DIR}/lib/libhubbub.so

.for _d in ${OBJS:.o=.d}
.sinclude "${OBJDIR}/${_d}"
.endfor
