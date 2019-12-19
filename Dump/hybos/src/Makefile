#MAKEFILE=ming.mak
MAKEFILE=Makefile

all:
	make -f $(MAKEFILE) -C kernel
	make -f $(MAKEFILE) -C mm
	make -f $(MAKEFILE) -C esh

install:
	make -f $(MAKEFILE) -C mm
	make -f $(MAKEFILE) -C esh
	make -f $(MAKEFILE) -C kernel install

clean:
	make -f $(MAKEFILE) -C kernel clean
	make -f $(MAKEFILE) -C mm clean
	make -f $(MAKEFILE) -C esh clean

