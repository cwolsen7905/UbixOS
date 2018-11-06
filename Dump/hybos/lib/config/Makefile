.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

CONFIG		=config.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(CONFIG)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\config.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
config.o:	config.c	$(MAKEDEP)

