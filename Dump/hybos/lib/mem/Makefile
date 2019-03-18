.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

MEM			=memccpy.o		\
				 memchr.o		\
				 memcmp.o		\
				 memcpy.o		\
				 memcpybw.o		\
				 memicmp.o		\
				 memmove.o		\
				 memset.o		\
				 memsetw.o		\
				 swab.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(MEM)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\memccpy.o
	del ..\$(OBJ_DIR)\memchr.o
	del ..\$(OBJ_DIR)\memcmp.o
	del ..\$(OBJ_DIR)\memcpy.o
	del ..\$(OBJ_DIR)\memcpybw.o
	del ..\$(OBJ_DIR)\memicmp.o
	del ..\$(OBJ_DIR)\memmove.o
	del ..\$(OBJ_DIR)\memset.o
	del ..\$(OBJ_DIR)\memsetw.o
	del ..\$(OBJ_DIR)\swab.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
memccpy.o:		memccpy.c	$(MAKEDEP)
memchr.o:		memchr.c		$(MAKEDEP)
memcmp.o:		memcmp.c		$(MAKEDEP)
memcpy.o:		memcpy.c		$(MAKEDEP)
memcpybw.o:		memcpybw.c	$(MAKEDEP)
memicmp.o:		memicmp.c	$(MAKEDEP)
memmove.o:		memmove.c	$(MAKEDEP)
memset.o:		memset.c		$(MAKEDEP)
memsetw.o:		memsetw.c	$(MAKEDEP)
swab.o:			swab.c		$(MAKEDEP)

