# The System Makefile (C) 2002, 2017 The UbixOS Project

.if ${.MAKEFLAGS:M${.CURDIR}/share/mk} == ""
.MAKEFLAGS: -m ${.CURDIR}/share/mk
.endif

.include <ubix.own.mk>

_SUBDIR=lib bin sys

.for dir in ${_SUBDIR}
.if (${BUILD_${dir}:Uyes} != "no" && exists(${dir}/Makefile))
SUBDIR+= ${dir}
.endif
.endfor

.include <ubix.obj.mk>
.include <ubix.kernobj.mk>
.include <ubix.subdir.mk>
