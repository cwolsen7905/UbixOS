.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

CHAR			=isalnum.o		\
				 isalpha.o		\
				 isascii.o		\
				 iscsym.o		\
				 iscsymf.o		\
				 isctrl.o		\
				 isdigit.o		\
				 isgraph.o		\
				 islower.o		\
				 isprint.o		\
				 ispunct.o		\
				 isspace.o		\
				 isupper.o		\
				 isxdigit.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(CHAR)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\isalnum.o
	del ..\$(OBJ_DIR)\isalpha.o
	del ..\$(OBJ_DIR)\isascii.o
	del ..\$(OBJ_DIR)\iscsym.o
	del ..\$(OBJ_DIR)\iscsymf.o
	del ..\$(OBJ_DIR)\isctrl.o
	del ..\$(OBJ_DIR)\isdigit.o
	del ..\$(OBJ_DIR)\isgraph.o
	del ..\$(OBJ_DIR)\islower.o
	del ..\$(OBJ_DIR)\isprint.o
	del ..\$(OBJ_DIR)\ispunct.o
	del ..\$(OBJ_DIR)\isspace.o
	del ..\$(OBJ_DIR)\isupper.o
	del ..\$(OBJ_DIR)\isxdigit.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
isalnum.o:	isalnum.c	$(MAKEDEP)
isalpha.o:	isalpha.c	$(MAKEDEP)
isascii.o:	isascii.c	$(MAKEDEP)
iscsym.o:	iscsym.c		$(MAKEDEP)
iscsymf.o:	iscsymf.c	$(MAKEDEP)
isctrl.o:	isctrl.c		$(MAKEDEP)
isdigit.o:	isdigit.c	$(MAKEDEP)
isgraph.o:	isgraph.c	$(MAKEDEP)
islower.o:	islower.c	$(MAKEDEP)
isprint.o:	isprint.c	$(MAKEDEP)
ispunct.o:	ispunct.c	$(MAKEDEP)
isspace.o:	isspace.c	$(MAKEDEP)
isupper.o:	isupper.c	$(MAKEDEP)
isxdigit.o:	isxdigit.c	$(MAKEDEP)

