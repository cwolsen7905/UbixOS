# bin/test/Makefile — test(1) and its "[" alias, built against musl.
#
# The FreeBSD test.c is used unmodified; compat/ supplies the few <sys/cdefs.h>
# bits musl lacks.  "[" is installed as a second binary (copy of test) because
# the image stages plain files and test.c keys its "[ ... ]" behaviour off
# argv[0].  eaccess(3), a GNU extension musl hides, maps to access(2): UbixOS
# has no setuid, so effective and real ids always match.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = test

OBJS = test.o

EXTRA_CFLAGS = -I${.CURDIR}/compat -Deaccess=access -w

.include "${UBIX_MK}/ubix.musl.prog.mk"

# The first target in the shared prog.mk is $(BINARY); make "all" the default so
# the "[" copy is produced too.
.MAIN: all
all: lbracket

.PHONY: lbracket
lbracket: $(BINARY)
	cp ${OBJ_DIR}/${BINDIR}/test "${OBJ_DIR}/${BINDIR}/["

clean: clean-lbracket
.PHONY: clean-lbracket
clean-lbracket:
	$(REMOVE) "${OBJ_DIR}/${BINDIR}/["
