# $Id$
# The System Makefile (C) 2002 The UbixOS Project

all: 
	(cd gen;make LIBPATH=${LIBPATH})
	(cd net;make LIBPATH=${LIBPATH})
	(cd stdlib;make LIBPATH=${LIBPATH})
	(cd string;make LIBPATH=${LIBPATH})
	(cd sys;make LIBPATH=${LIBPATH})
	(cd syscalls;make LIBPATH=${LIBPATH})

# Clean up the junk
clean:
	(cd gen;make clean)
	(cd net;make clean)
	(cd stdlib;make clean)
	(cd string;make clean)
	(cd sys;make clean)
	(cd syscalls;make clean)

