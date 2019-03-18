.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

X86			=disable.o		\
				 enable.o		\
				 inportb.o		\
				 outportb.o		\
				 inportw.o		\
				 outportw.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(X86)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\disable.o
	del ..\$(OBJ_DIR)\enable.o
	del ..\$(OBJ_DIR)\inportb.o
	del ..\$(OBJ_DIR)\outportb.o
	del ..\$(OBJ_DIR)\inportw.o
	del ..\$(OBJ_DIR)\outportw.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
disable.o:		disable.c		$(MAKEDEP)
enable.o:		enable.c			$(MAKEDEP)
inportb.o:		inportb.c		$(MAKEDEP)
outportb.o:		outportb.c		$(MAKEDEP)
inportw.o:		inportw.c		$(MAKEDEP)
outportw.o:		outportw.c		$(MAKEDEP)

