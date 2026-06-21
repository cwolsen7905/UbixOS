# Host build for the UbixFS macOS-kit C foundation (vdev shim + read-path test).
# Plain host program linking the portable UbixFS core (lib/ubixfs_core) — no
# cross compiler, no kernel.  See docs/design/ubixfs-mac-browser-plan.md.
#
#   make            build image_test
#   make test       build + run the end-to-end read-path check (raw + MBR images)
#   make clean

.CURDIR ?= ${.PARSEDIR}
CORE     = ${.CURDIR}/../../../lib/ubixfs_core
INC      = ${.CURDIR}/../../../include/fs/ubixfs
UBFS     = ${.CURDIR}/../../ubixfs/ubfs

HOSTCC  ?= cc
CFLAGS  += -O2 -g -Wall -Wextra -std=c11 -I${.CURDIR} -I${CORE} -I${INC} \
           -idirafter ${.CURDIR}/../../../include

CORE_SRCS = ${CORE}/fletcher.c ${CORE}/ubfs_spa.c ${CORE}/ubfs_dmu.c \
            ${CORE}/ubfs_dir.c ${CORE}/ubfs_dsl.c ${CORE}/ubfs_fs.c
KIT_SRCS  = ${.CURDIR}/ubfs_vdev_image.c
# The Obj-C facade Swift consumes (also compilable as a plain host program).
OBJC_SRCS = ${.CURDIR}/UbixFSEntry.m ${.CURDIR}/UbixFSSession.m
OBJCFLAGS = ${CFLAGS} -fobjc-arc -framework Foundation

LIB       = libubixfskit.a
LIB_SRCS  = ${KIT_SRCS} ${CORE_SRCS} ${OBJC_SRCS}

# Static library the SwiftUI app links (C core + vdev shim + Obj-C facade).
lib: ${LIB}

${LIB}: ${LIB_SRCS}
	rm -f ${LIB} *.o
	${HOSTCC} ${CFLAGS} -fobjc-arc -c ${LIB_SRCS}
	ar rcs ${LIB} *.o
	rm -f *.o

image_test: image_test.c ${KIT_SRCS} ${CORE_SRCS}
	${HOSTCC} ${CFLAGS} -o ${.TARGET} ${.ALLSRC}

objc_test: objc_test.m ${OBJC_SRCS} ${KIT_SRCS} ${CORE_SRCS}
	${HOSTCC} ${OBJCFLAGS} -o ${.TARGET} ${.ALLSRC}

# End-to-end check: build a populated raw pool with the ubfs CLI, then wrap a
# copy of it in an MBR disk image, and read both back through the shim; then run
# the Obj-C facade (write-back round trip) against the raw pool.
test: image_test objc_test
	@sh ${.CURDIR}/run_test.sh "${UBFS}" ./image_test
	@echo; echo "== exercising the Obj-C facade (write-back) =="
	@./objc_test ./raw.pool

clean:
	rm -f image_test objc_test ${LIB} *.o raw.pool disk.img
