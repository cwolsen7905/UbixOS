# Makefile (C) 2017 The UbixOS Project

all: ld-bin #rtld-elf-bin

ld-bin: ld
	(cd ld;make)

rtld-elf-bin: rtld-elf
	(cd rtld-elf;make)

clean:
	(cd ld;make clean)
#	(cd rtld-elf;make clean)
