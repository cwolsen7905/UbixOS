# The System Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

OBJS =

# Object files are written to ${OBJ_DIR}/lib/<sub-dir-basename>/ by ubix.sublib.mk
SUBS = \
	${OBJ_DIR}/obj/lib/stdio/*.o \
	${OBJ_DIR}/obj/lib/sys/*.o \
	${OBJ_DIR}/obj/lib/string/*.o \
	${OBJ_DIR}/obj/lib/stdlib/*.o \
	${OBJ_DIR}/obj/lib/math/*.o \
	${OBJ_DIR}/obj/lib/quad/*.o \
	${OBJ_DIR}/obj/lib/generic/*.o \
	${OBJ_DIR}/obj/lib/locale/*.o \
	${OBJ_DIR}/obj/lib/gen/*.o \
	${OBJ_DIR}/obj/lib/dirent/*.o

OUTPUT = libc

lib.so: $(OBJS)
	(cd stdio;$(MAKE))
	(cd stdlib;$(MAKE))
	(cd math;$(MAKE))
	(cd locale;$(MAKE))
	(cd quad;$(MAKE))
	(cd sys;$(MAKE))
	(cd string;$(MAKE))
	(cd generic;$(MAKE))
	(cd gen;$(MAKE))
	(cd dirent;$(MAKE))
	${CROSS_PREFIX}ld -m elf_i386 -shared -x --soname=$(OUTPUT).so -o ${BUILD_DIR}/lib/$(OUTPUT).so `lorder $(SUBS) | tsort`
	${CROSS_PREFIX}ar rcs ${BUILD_DIR}/lib/$(OUTPUT).a `lorder $(SUBS) | tsort`

clean:
	$(REMOVE) $(OBJS) ${BUILD_DIR}/lib/${OUTPUT}.so ${BUILD_DIR}/lib/${OUTPUT}.a
	(cd stdio;${MAKE} clean)
	(cd sys;${MAKE} clean)
	(cd stdlib;${MAKE} clean)
	(cd locale;${MAKE} clean)
	(cd generic;${MAKE} clean)
	(cd string;${MAKE} clean)
	(cd math;${MAKE} clean)
	(cd quad;${MAKE} clean)
	(cd gen;${MAKE} clean)
	(cd dirent;${MAKE} clean)
