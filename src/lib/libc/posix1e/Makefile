# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
#include ../../Makefile.inc
#include ../Makefile.inc
include ../libc.inc

#INCLUDES += -I../locale/

#Objects
#OBJS = acl_calc_mask.o acl_copy.o acl_delete.o acl_delete_entry.o acl_entry.o acl_free.o acl_from_text.o acl_get.o acl_init.o acl_perm.o acl_set.o acl_size.o acl_support.o acl_to_text.o acl_valid.o extattr.o mac.o mac_exec.o mac_get.o mac_set.o

OBJS = acl_calc_mask.o acl_copy.o acl_delete.o acl_delete_entry.o acl_entry.o acl_free.o acl_from_text.o acl_get.o acl_init.o acl_perm.o acl_set.o acl_support.o acl_to_text.o acl_valid.o extattr.o mac.o mac_exec.o mac_get.o mac_set.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

