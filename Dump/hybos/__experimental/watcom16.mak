# Makefile for DOS - Watcom C

# defines
# MAKEDEP	=watcom16.mak
CC	=wcc    -3 -s -d2 -hw -ox -w=9 -zc -ms
OBJS	=demo.obj dos.obj dos16.obj

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

dos16.o:	dos16.c		$(MAKEDEP) diskio.h dos.h

# explicit rules
diskio.exe: $(OBJS) $(MAKEDEP)
	wlink D W A SYSTEM dos NAME $@ FILE demo.obj FILE dos.obj FILE dos16.obj

