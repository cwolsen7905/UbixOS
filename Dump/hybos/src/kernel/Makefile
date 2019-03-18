.SUFFIXES: .asm

# defines
MAKEFILE		=Makefile
MAKEDEP		=$(MAKEFILE)
INCDIR		=../../include
LDSCRIPT		=../krnl1m.ld
NASM			=nasm -f elf -i$(INCDIR)/
CC				=gcc -g -Wall -W -O2 -nostdinc -fno-builtin -I$(INCDIR)
LD				=ld -g -T $(LDSCRIPT) -nostdlib
#LD				=D:/cygwin/usr/cross/bin/i586-elf-ld.exe -g -T $(LDSCRIPT) -nostdlib
LIBC			=../../lib/libc.a

OBJS			=debug.o					\
				 main.o					\
				 kstart.o				\
				 keyboard.o				\
				 sched.o					\
				 tasks.o					\
				 video.o					\
				 bootlog.o
#				 cpu.o
OBJS_DEP		=*.o

OBJ_DIR		=objects

# targets
all: kernel.bin

install: kernel.bin
	copy ..\..\boot\kernel.bin a:\kernel.bin

clean:
	del ..\$(OBJ_DIR)\debug.o
	del ..\$(OBJ_DIR)\keyboard.o
	del ..\$(OBJ_DIR)\kstart.o
	del ..\$(OBJ_DIR)\main.o
	del ..\$(OBJ_DIR)\sched.o
	del ..\$(OBJ_DIR)\tasks.o
	del ..\$(OBJ_DIR)\video.o
	del ..\..\boot\kernel.bin
	del ..\$(OBJ_DIR)\kernel.dis
	del ..\$(OBJ_DIR)\kernel.sym
	del ..\$(OBJ_DIR)\bootlog.o
#	del ..\$(OBJ_DIR)\cpu.o

# implicit rules
.asm.o:
	$(NASM) -o../$(OBJ_DIR)/$@ $<

.c.o:
	$(CC) -c -o../$(OBJ_DIR)/$@ $<

# dependencies
kstart.o:	kstart.asm	$(MAKEDEP)
main.o:		main.c		$(MAKEDEP)
video.o:		video.c		$(MAKEDEP)
debug.o:		debug.c		$(MAKEDEP)
kbd.o:		kbd.c			$(MAKEDEP)
sched.o:		sched.c		$(MAKEDEP)
tasks.o:		tasks.c		$(MAKEDEP)
bootlog.o:	bootlog.c	$(MAKEDEP)
#cpu.o:		cpu.c			$(MAKEDEP)

# explicit rules
$(LIBC): ../../lib/$(MAKEFILE)
	make -C ../../lib -f $(MAKEFILE)

kernel.bin: $(OBJS) $(LDSCRIPT) $(LIBC)	$(MAKEDEP)
	$(LD) -o../../boot/$@ ../$(OBJ_DIR)/*.o $(LIBC)
	objdump --source ../../boot/$@ >../$(OBJ_DIR)/kernel.dis
	nm --line-numbers ../../boot/$@ | sort >../$(OBJ_DIR)/kernel.sym
	strip ../../boot/$@
