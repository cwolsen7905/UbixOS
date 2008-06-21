# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
include ../../Makefile.inc
#include ../Makefile.inc
#include ../libc.inc

INCLUDES = -I../../include.new -I../libc/gen/

OUTPUT = libutil.so

CFLAGS = -fpic -DPIC -O2 -fno-strict-aliasing -pipe -D__DBINTERFACE_PRIVATE -DINET6 -DPOSIX_MISTAKE -DBROKEN_DES -DPORTMAP -DDES_BUILTIN -DYP -DHESIOD -Wsystem-headers -Wall -Wno-format-y2k -Wno-uninitialized 

OBJS = _secure_path.o gr_util.o login_cap.o login_tty.o pty.o uucplock.o auth.o humanize_number.o login_class.o logout.o pw_util.o expand_number.o kld.o login_crypt.o logwtmp.o realhostname.o flopen.o login.o login_ok.o pidfile.o stub.o fparseln.o login_auth.o login_times.o property.o trimdomain.o

all: $(OBJS)
	$(CC) -fnobuiltin -nostdlib -shared -Wl,-x -o $(OUTPUT) -Wl,-soname,$(OUTPUT) `lorder $(OBJS) $(SUBS) | tsort -q`

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

