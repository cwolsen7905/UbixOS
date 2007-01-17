# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../../../Makefile.inc
include ../../Makefile.inc
include ../Makefile.inc

INCLUDES += -I../locale/

#Objects
OBJS = big5.o btowc.o collate.o collcmp.o euc.o fix_grouping.o gb18030.o gb2312.o gbk.o isctype.o iswctype.o ldpart.o lmessages.o lmonetary.o lnumeric.o localeconv.o mblen.o mbrlen.o mbrtowc.o mbsinit.o mbsnrtowcs.o mbsrtowcs.o mbstowcs.o mbtowc.o mskanji.o nextwctype.o nl_langinfo.o nomacros.o none.o rpmatch.o rune.o runetype.o setlocale.o setrunelocale.o table.o tolower.o toupper.o utf8.o wcrtomb.o wcsftime.o wcsnrtombs.o wcsrtombs.o wcstod.o wcstof.o wcstoimax.o wcstol.o wcstold.o wcstoll.o wcstombs.o wcstoul.o wcstoull.o wcstoumax.o wctob.o wctomb.o wctrans.o wctype.o wcwidth.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) -Wall -nostdlib -O $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -Wall -nostdlib -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

