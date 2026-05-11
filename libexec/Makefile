# Makefile (C) 2017 The UbixOS Project

all: ld-bin #rtld-elf-bin

ld-bin: ld
	(cd ld;$(MAKE))

rtld-elf-bin: rtld-elf
	(cd rtld-elf;$(MAKE))

clean:
	(cd ld;${MAKE} clean)
#	(cd rtld-elf;make clean)
