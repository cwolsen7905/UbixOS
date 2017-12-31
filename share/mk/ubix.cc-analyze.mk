.ifndef CC_ANALYZE_SRCS

CC_ANALYZE_FLAGS+=	--analyze

CC_ANALYZE_CHECKERS+=	core deadcode security unix

.for checker in ${CC_ANALYZE_CHECKERS}
CC_ANALYZE_FLAGS+=	-Xanalyzer -analyzer-checker=${checker}
.endfor

.SUFFIXES: .c .cc .cpp .cxx .C .cc-analyzer

CC_ANALYZE_CFLAGS=		${CFLAGS:N-Wa,--fatal-warnings}
CC_ANALYZE_CXXFLAGS=	${CXXFLAGS:N-Wa,--fatal-warnings}

.c.cc-analyzer:
	${TOOL_CC.cc} ${CC_ANALYZE_FLAGS} \
	    ${CC_ANALYZE_CFLAGS} ${CPPFLAGS} \
	    ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} \
	    ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}
.cc.cc-analyzer .cpp.clang-analyzer .cxx.clang-analyzer .C.clang-analyzer:
	${TOOL_CXX.cc} ${CC_ANALYZE_FLAGS} \
	    ${CC_ANALYZE_CXXFLAGS} ${CPPFLAGS} \
	    ${COPTS.${.IMPSRC:T}} ${CPUFLAGS.${.IMPSRC:T}} \
	    ${CPPFLAGS.${.IMPSRC:T}} ${.IMPSRC}

CC_ANALYZE_SRCS= \
	${SRCS:M*.[cC]} ${SRCS:M*.cc} \
	${SRCS:M*.cpp} ${SRCS:M*.cxx} \
	${DPSRCS:M*.[cC]} ${DPSRCS:M*.cc} \
	${DPSRCS:M*.cpp} ${DPSRCS:M*.cxx}
.if !empty(CC_ANALYZE_SRCS)
CC_ANALYZE_OUTPUT=	${CC_ANALYZE_SRCS:R:S,$,.cc-analyzer,}
.endif

analyze: ${CC_ANALYZE_OUTPUT}

.endif
