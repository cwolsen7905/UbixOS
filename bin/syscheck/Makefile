# Application Makefile (C) 2002-2026 The UbixOS Project

include ../../Makefile.incl
include ../Makefile.incl

BINARY    = syscheck
REMOVE    = rm -f
OBJS      = main.o
LIBRARIES = ../../build/lib/libc.so

$(BINARY): $(OBJS)
	${CROSS_PREFIX}ld -m elf_i386 -e _start -dynamic-linker sys:/libexec/ld.so \
		-o ${BUILD_DIR}/bin/$@ $(STARTUP) $(OBJS) $(LIBRARIES)

.c.o:
	$(CC) -Wall -O $(CFLAGS) $(INCLUDES) -c -o $@ $<

.S.o:
	$(CC) -Wall $(CFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	$(REMOVE) $(OBJS) $(BUILD_DIR)/bin/$(BINARY)

.for _d in ${OBJS:.o=.d}
.sinclude "${_d}"
.endfor