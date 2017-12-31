.include <ubix.init.mk>

.if !defined(NOSUBDIR)

.for dir in ${SUBDIR}

.if "${dir:H}" != ""

.if exists(${.CURDIR}/${dir}.${MACHINE})
__REALSUBDIR+=${dir}.${MACHINE}
.else
__REALSUBDIR+=${dir}
.endif

.else
.if exists(${dir}.${MACHINE})
__REALSUBDIR+=${dir}.${MACHINE}
.else
__REALSUBDIR+=${dir}
.endif

.endif
.endfor

__recurse: .USE
	@${MAKEDIRTARGET} ${.TARGET:C/^[^-]*-//} ${.TARGET:C/-.*$//}

.if make(cleandir)
__RECURSETARG=  ${TARGETS:Nclean}
clean:
.else
__RECURSETARG=  ${TARGETS}
.endif

.for targ in ${__RECURSETARG}

.for dir in ${__REALSUBDIR}
__TARGDIR := ${dir}
.if !commands(${targ}-${dir})
${targ}-${dir}: .PHONY .MAKE __recurse
SUBDIR_${targ} += ${targ}-${dir}
.endif
.endfor

subdir-${targ}: .PHONY ${SUBDIR_${targ}}
${targ}: subdir-${targ}
.endfor

.endif

${TARGETS}:
