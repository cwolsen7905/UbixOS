#	$UBIXBSD: bsd.shlib.mk,v 1.8 2012/08/23 21:21:17 joerg Exp $

.if !defined(_UBIX_SHLIB_MK_)
_UBIX_SHLIB_MK_=1

.if ${MKDYNAMICROOT} == "no"
SHLIBINSTALLDIR?= /ubixos/usr/lib
.else
SHLIBINSTALLDIR?= /ubixos/lib
.endif

.if ${MKDYNAMICROOT} == "no" || \
    (${BINDIR:Ux} != "/bin" && ${BINDIR:Ux} != "/sbin" && \
     ${BINDIR:Ux} != "/libexec" && ${USE_SHLIBDIR:Uno} == "no")
SHLIBDIR?=	/ubixos/usr/lib
.else
SHLIBDIR?=	/ubixos/lib
.endif

.if ${USE_SHLIBDIR:Uno} != "no"
_LIBSODIR?=	${SHLIBINSTALLDIR}
.else
_LIBSODIR?=	${LIBDIR}
.endif

.if ${MKDYNAMICROOT} == "no"
SHLINKINSTALLDIR?= /ubixos/usr/libexec
.else
SHLINKINSTALLDIR?= /ubixos/libexec
.endif

.if ${MKDYNAMICROOT} == "no" || \
    (${BINDIR:Ux} != "/bin" && ${BINDIR:Ux} != "/sbin" && \
     ${BINDIR:Ux} != "/libexec")
SHLINKDIR?=	/ubixos/usr/libexec
.else
SHLINKDIR?=	/ubixos/libexec
.endif

.endif	# !defined(_UBIX_SHLIB_MK_)
