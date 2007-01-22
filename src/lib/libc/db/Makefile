# $Id$
# The System Makefile (C) 2002 The UbixOS Project

all: 
	(cd btree;make LIBPATH=${LIBPATH})
	(cd db;make LIBPATH=${LIBPATH})
	(cd hash;make LIBPATH=${LIBPATH})
	(cd mpool;make LIBPATH=${LIBPATH})
	(cd recno;make LIBPATH=${LIBPATH})

# Clean up the junk
clean:
	(cd btree;make clean)
	(cd db;make clean)
	(cd hash;make clean)
	(cd mpool;make clean)
	(cd recno;make clean)
