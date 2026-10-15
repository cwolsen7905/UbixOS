# stand/ -- boot loaders.  `bmake -C stand TARGET=x86_64` builds
# build/x86_64/boot/BOOTX64.EFI (clang + lld-link, UEFI target; see
# share/mk/ubix.efi.mk).  aarch64 follows in B2 (W-055).
TARGET?=	x86_64

all:
.if ${TARGET} == "x86_64"
	${MAKE} -C ${.CURDIR}/efi/x86_64
.else
	@echo "stand: no UEFI loader for ${TARGET} yet"
.endif

clean:
	${MAKE} -C ${.CURDIR}/efi/x86_64 clean
