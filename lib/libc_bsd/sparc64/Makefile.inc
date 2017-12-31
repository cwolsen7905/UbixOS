Makefile.incl 246117 2013-01-30 12:48:16Z kib $
#
# Machine dependent definitions for the ultra sparc architecture.
#

Makefile.incl"

SRCS+=	__vdso_gettc.c

# Long double is quad precision
GDTOASRCS+=strtorQ.c
MDSRCS+=machdep_ldisQ.c
SYM_MAPS+=${.CURDIR}/sparc64/Symbol.map
