.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

MATH			=abs.o		\
				 max.o		\
				 min.o		\
				 rotl.o		\
				 rotr.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(MATH)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\abs.o
	del ..\$(OBJ_DIR)\max.o
	del ..\$(OBJ_DIR)\min.o
	del ..\$(OBJ_DIR)\rotl.o
	del ..\$(OBJ_DIR)\rotr.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
abs.o:	abs.c		$(MAKEDEP)
max.o:	max.c		$(MAKEDEP)
min.o:	min.c		$(MAKEDEP)
rotl.o:	rotl.c	$(MAKEDEP)
rotr.o:	rotr.c	$(MAKEDEP)

