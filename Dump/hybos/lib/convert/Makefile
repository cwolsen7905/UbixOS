.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
NASM			=nasm -f win32 -dUNDERBARS=1 -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)

CONVERT		=atoi16.o		\
				 atoi32.o		\
				 atoi64.o		\
				 atoi.o			\
				 i16toa.o		\
				 i32toa.o		\
				 i64toa.o		\
				 itoa.o

# This will be one level above where we are now
OBJ_DIR		=objects

OBJS			=$(CONVERT)

# targets
all: $(OBJS) $(MAKEDEP)

clean:
	del ..\$(OBJ_DIR)\atoi16.o
	del ..\$(OBJ_DIR)\atoi32.o
	del ..\$(OBJ_DIR)\atoi64.o
	del ..\$(OBJ_DIR)\atoi.o
	del ..\$(OBJ_DIR)\i16toa.o
	del ..\$(OBJ_DIR)\i32toa.o
	del ..\$(OBJ_DIR)\i64toa.o
	del ..\$(OBJ_DIR)\itoa.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
atoi.o:		atoi.c		$(MAKEDEP)
itoa.o:		itoa.c		$(MAKEDEP)
atoi16.o:	atoi16.c		$(MAKEDEP)
atoi32.o:	atoi32.c		$(MAKEDEP)
atoi64.o:	atoi64.c		$(MAKEDEP)
i16toa.o:	i16toa.c		$(MAKEDEP)
i32toa.o:	i32toa.c		$(MAKEDEP)
i64toa.o:	i64toa.c		$(MAKEDEP)
u16toa.o:	u16toa.c		$(MAKEDEP)
u32toa.o:	u32toa.c		$(MAKEDEP)
u64toa.o:	u64toa.c		$(MAKEDEP)

