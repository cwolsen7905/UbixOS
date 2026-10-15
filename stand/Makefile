# stand/ -- boot loaders.  `bmake -C stand TARGET=x86_64|aarch64` builds
# build/<arch>/boot/BOOTX64.EFI or BOOTAA64.EFI (clang + lld-link, UEFI target;
# see share/mk/ubix.efi.mk).
TARGET?=	x86_64

all:
.if ${TARGET} == "x86_64" || ${TARGET} == "aarch64"
	${MAKE} -C ${.CURDIR}/efi/${TARGET}
.else
	@echo "stand: no UEFI loader for ${TARGET}"
.endif

clean:
	${MAKE} -C ${.CURDIR}/efi/x86_64 clean
	${MAKE} -C ${.CURDIR}/efi/aarch64 clean
