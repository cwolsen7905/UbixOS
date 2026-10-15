# BOOTAA64.EFI -- uBix UEFI loader, aarch64.  Run as: bmake -C stand/efi/aarch64
EFI_ARCH=	aarch64
EFI_NAME=	BOOTAA64.EFI
EFI_OUT?=	${.CURDIR}/../../../build/aarch64/boot
EFI_SRCS=	${.CURDIR}/../loader.c ${.CURDIR}/../lib.c ${.CURDIR}/md.c ${.CURDIR}/handoff.S

all: ${EFI_OUT}/${EFI_NAME}

clean:
	rm -rf ${EFI_OUT}/${EFI_NAME} ${EFI_OUT}/obj

.include "${.CURDIR}/../../../share/mk/ubix.efi.mk"
