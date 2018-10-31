# Makefile for 32-bit DOS - Watcom C
# (using CauseWay DOS extender)
# xxx - the resulting DISKIO.EXE doesn't work (!)

# defines
# MAKEDEP	=watcom32.mak
CC	=wcc386 -3 -s -d2 -hw -ox -w=9 -zc -mf
OBJS	=demo.obj dos.obj watcom32.obj

# targets
all: diskio.exe

clean:
	deltree /y *.exe *.obj *.o *.err

# implicit rules
.c.obj:
	$(CC) -fo=$@ $[.

# dependencies
demo.o:		demo.c		$(MAKEDEP) diskio.h

dos.o:		dos.c		$(MAKEDEP) diskio.h dos.h

watcom32.o:	watcom32.c	$(MAKEDEP) diskio.h dos.h

# explicit rules
diskio.exe: $(OBJS) $(MAKEDEP)
	wlink D W A SYSTEM causeway NAME $@ FILE demo.obj FILE dos.obj FILE watcom32.obj

