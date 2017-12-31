.if !defined(_UBIX_INIT_MK_)
_UBIX_INIT_MK_=1

.-include "${.CURDIR}/../Makefile.inc"
.include <ubix.own.mk>
.MAIN:          all

.endif  # !defined(_UBIX_INIT_MK_)

