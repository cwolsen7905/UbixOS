.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

STDIO			=doprintf.o			\
				 sprintf.o			\
				 printf.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(STDIO)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\doprintf.o
	del ..\$(OBJ_DIR)\sprintf.o
	del ..\$(OBJ_DIR)\printf.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
doprintf.o:	doprintf.c	$(MAKEDEP)
sprintf.o:	sprintf.c	$(MAKEDEP)
printf.o:	printf.c		$(MAKEDEP)

