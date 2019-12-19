#MAKEFILE=ming.mak
MAKEFILE		=Makefile

OBJS			=objects/*.o

all:
	make -f $(MAKEFILE) -C char
	make -f $(MAKEFILE) -C config
	make -f $(MAKEFILE) -C convert
	make -f $(MAKEFILE) -C ctype
	make -f $(MAKEFILE) -C math
	make -f $(MAKEFILE) -C mem
	make -f $(MAKEFILE) -C setjmp
	make -f $(MAKEFILE) -C stdio
	make -f $(MAKEFILE) -C string
	make -f $(MAKEFILE) -C util
	make -f $(MAKEFILE) -C x86
	ar rcs libc.a $(OBJS)

clean:
	make -f $(MAKEFILE) -C char clean
	make -f $(MAKEFILE) -C config clean
	make -f $(MAKEFILE) -C convert clean
	make -f $(MAKEFILE) -C ctype clean
	make -f $(MAKEFILE) -C math clean
	make -f $(MAKEFILE) -C mem clean
	make -f $(MAKEFILE) -C setjmp clean
	make -f $(MAKEFILE) -C stdio clean
	make -f $(MAKEFILE) -C string clean
	make -f $(MAKEFILE) -C util clean
	make -f $(MAKEFILE) -C x86 clean
	del libc.a

