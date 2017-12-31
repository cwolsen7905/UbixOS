.include <ubix.own.mk>

CSU_MACHINE_ARCH?=      ${MACHINE_ARCH}
CSU_MACHINE_CPU?=       ${MACHINE_CPU}

.if !empty(CSU_MACHINE_ARCH:Mearm*)
ARCHDIR:=       ${.CURDIR}/arch/earm
.elif exists(${.CURDIR}/arch/${CSU_MACHINE_ARCH}/Makefile.incl)
ARCHDIR:=       ${.CURDIR}/arch/${CSU_MACHINE_ARCH}
.elif exists(${.CURDIR}/arch/${CSU_MACHINE_CPU}/Makefile.incl)
ARCHDIR:=       ${.CURDIR}/arch/${CSU_MACHINE_CPU}
.else
.error Architecture (${CSU_MACHINE_ARCH} or ${CSU_MACHINE_CPU}) unsupported
.endif


.PATH:  ${ARCHDIR}
.include "${ARCHDIR}/Makefile.incl"
.include "${.CURDIR}/common/Makefile.incl"
