# Makefile for 32-bit DOS - DJGPP

# defines
# MAKEDEP	=makefile
CC	=gcc -g -O2 -Wall -W
LD	=gcc -g
OBJS	=demo.o dos.o djgpp.o

# targets
all: diskio.exe

clean:
	deltree /y *.exe *.obj *.o *.err

# implicit rules
.c.o:
	$(CC) -c -o$@ $<

# dependencies
demo.o:		demo.c		$(MAKEDEP) diskio.h

dos.o:		dos.c		$(MAKEDEP) diskio.h dos.h

djgpp.o:	djgpp.c		$(MAKEDEP) diskio.h dos.h

# explicit rules
diskio.exe: $(OBJS) $(MAKEDEP)
	$(LD) -o$@ $(OBJS)
