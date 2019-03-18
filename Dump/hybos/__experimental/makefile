# Makefile for Turbo C++ 1.0
# For old Turbo MAKE 3.0, this file MUST be named 'makefile'

# defines
# MAKEDEP	=makefile
MODEL	=s
CC	=tcc -v -m$(MODEL) -w -O2 -d -Z -vi -1
LIBDIR	=c:\tc\lib
LD	=tlink /v /x
OBJS	=demo.obj dos.obj dos16.obj

# targets
all: diskio.exe

clean:
	deltree /y *.exe *.obj *.o *.err

# implicit rules
.c.obj:
	$(CC) -c -o$*.obj $<

# dependencies
demo.o:		demo.c		$(MAKEDEP) diskio.h

dos.o:		dos.c		$(MAKEDEP) diskio.h dos.h

dos16.o:	dos16.c		$(MAKEDEP) diskio.h dos.h

# explicit rules
diskio.exe: $(OBJS) $(MAKEDEP)
	$(LD) $(LIBDIR)\c0$(MODEL).obj $(OBJS),$.,,$(LIBDIR)\c$(MODEL).lib

