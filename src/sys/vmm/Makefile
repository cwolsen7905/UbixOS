#  (C) 2002 The UbixOS Project
#  $Id$

# Include Global 'Source' Options
include ../../Makefile.inc
include ../Makefile.inc

#  Objects
OBJS = page_fault.o pagefault.o getfreevirtualpage.o copyvirtualspace.o setpageattributes.o unmappage.o getphysicaladdr.o getfreepage.o createvirtualspace.o vmm_memory.o paging.o vmm_init.o

all: $(OBJS)

#  Compile Types
.cc.o:
	$(CXX) ${CFLAGS} $(INCLUDES) -c -o $@ $<
.cc.s:
	$(CXX) ${CFLAGS} $(INCLUDES) -S -o $@ $<
.c.o:
	$(CC)  ${CFLAGS} $(INCLUDES) -c -o $@ $<
.c.s:
	$(CC)  ${CFLAGS} $(INCLUDES) -S -o $@ $<
.S.o:
	$(CC)  ${CFLAGS} $(INCLUDES) -c -o $@ $<

# Clean up the junk
clean:
	$(REMOVE) $(OBJS)
