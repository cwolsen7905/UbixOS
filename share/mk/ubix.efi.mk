# ubix.efi.mk -- build rules for the UEFI loader (stand/efi), PE32+ via clang +
# lld-link.  Included by stand/efi/<arch>/Makefile.  Inputs:
#   EFI_ARCH   x86_64 | aarch64      EFI_OUT   output directory
#   EFI_SRCS   sources (absolute)    EFI_NAME  e.g. BOOTX64.EFI
.if ${EFI_ARCH} == "x86_64"
EFI_TRIPLE=	x86_64-unknown-windows
EFI_MACHINE=	x64
EFI_CFLAGS=	-mno-red-zone -mno-sse -mno-mmx -mno-80387
.elif ${EFI_ARCH} == "aarch64"
EFI_TRIPLE=	aarch64-unknown-windows
EFI_MACHINE=	arm64
EFI_CFLAGS=	-mgeneral-regs-only
.endif

EFI_CC?=	clang
EFI_LINK?=	lld-link
EFI_CFLAGS+=	--target=${EFI_TRIPLE} -ffreestanding -fshort-wchar -fno-stack-protector -fno-builtin \
		-fno-pic -fno-unwind-tables -fno-asynchronous-unwind-tables -std=c11 -O2 -Wall -Wextra \
		-Wno-unused-parameter -I${.CURDIR}/../.. -I${.CURDIR}/../../../sys/include
EFI_OBJS=	${EFI_SRCS:T:R:S/$/.o/}

${EFI_OUT}/${EFI_NAME}: ${EFI_SRCS}
	mkdir -p ${EFI_OUT}/obj
.for s in ${EFI_SRCS}
	${EFI_CC} ${EFI_CFLAGS} -c ${s} -o ${EFI_OUT}/obj/${s:T:R}.o
.endfor
	${EFI_LINK} -subsystem:efi_application -entry:efi_main -nodefaultlib -machine:${EFI_MACHINE} \
		-nologo -out:${EFI_OUT}/${EFI_NAME} ${EFI_SRCS:T:R:S|^|${EFI_OUT}/obj/|:S|$|.o|}
