# Makefile for Windows NT - MinGW/CygWin

# defines
# MAKEDEP	=makefile
CC	=gcc -g -O2 -Wall -W
LD	=gcc -g
OBJS	=demo.o win-nt.o

# targets
all: diskio.exe

clean:
	deltree /y *.exe *.obj *.o *.err

# implicit rules
.c.o:
	$(CC) -c -o$@ $<

# dependencies
demo.o:		demo.c		$(MAKEDEP) diskio.h

win-nt.o:	win-nt.c	$(MAKEDEP) diskio.h

# explicit rules
diskio.exe: $(OBJS) $(MAKEDEP)
	$(LD) -o$@ $(OBJS)
