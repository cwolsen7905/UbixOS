.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
LDSCRIPT		=../../krnl1m.ld
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)
LD				=ld -g -T $(LDSCRIPT) -nostdlib
LIBC			=../../lib/libc.a

OBJS			=memory.o

OBJS_DEP		=*.o

OBJ_DIR		=objects

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\memory.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
memory.o:	memory.c		$(MAKEDEP)

