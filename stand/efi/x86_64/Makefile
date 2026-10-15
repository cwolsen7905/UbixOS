# BOOTX64.EFI -- uBix UEFI loader, x86_64.  Run as: bmake -C stand/efi/x86_64
EFI_ARCH=	x86_64
EFI_NAME=	BOOTX64.EFI
EFI_OUT?=	${.CURDIR}/../../../build/x86_64/boot
EFI_SRCS=	${.CURDIR}/../loader.c ${.CURDIR}/../lib.c ${.CURDIR}/md.c ${.CURDIR}/handoff.S

all: ${EFI_OUT}/${EFI_NAME}

clean:
	rm -rf ${EFI_OUT}/${EFI_NAME} ${EFI_OUT}/obj

.include "${.CURDIR}/../../../share/mk/ubix.efi.mk"
