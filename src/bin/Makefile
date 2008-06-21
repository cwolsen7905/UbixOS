# $Id$
# The System Makefile (C) 2002 The UbixOS Project

all: init-bin login-bin shell-bin clock-bin cp-bin fdisk-bin format-bin disklabel-bin ubistry-bin edit-bin ld-bin ttyd-bin stat-bin cat-bin rtld-elf-bin tcc-bin ls-bin
# test-bin pwd-bin cat-bin de-bin goofball-bin mount-bin

init-bin: init 
	(cd init;make)

login-bin: login
	(cd login;make)

shell-bin: shell
	(cd shell;make)

test-bin: test
	(cd test;make)

ls-bin: ls
	(cd ls;make)

pwd-bin: pwd
	(cd pwd;make)

cat-bin: cat
	(cd cat;make)

ld-bin: ld
	(cd ld;make)

de-bin: de
	(cd de;make)

muffin-bin: muffin
	(cd muffin;make)

goofball-bin: goofball
	(cd goofball;make)

clock-bin: clock
	(cd clock;make)

fdisk-bin: fdisk
	(cd fdisk;make)

cp-bin: cp
	(cd cp;make)

format-bin: format
	(cd format;make)

disklabel-bin: disklabel
	(cd disklabel;make)

launcher-bin: launcher
	(cd launcher;make)

ubistry-bin: ubistry
	(cd ubistry;make)

edit-bin: edit
	(cd edit;make)

ttyd-bin: ttyd
	(cd ttyd;make)

stat-bin: stat
	(cd stat;make)

mount-bin: mount
	(cd mount;make)

rtld-elf-bin: rtld-elf
	(cd rtld-elf;make)

tcc-bin: tcc
	(cd tcc;make)

clean:
	(cd cp;make clean)
	(cd fdisk;make clean)
	(cd init;make clean)
	(cd shell;make clean)
	(cd ls;make clean)
	(cd login;make clean)
	(cd clock;make clean)
	(cd muffin;make clean)
	(cd format;make clean)
	(cd disklabel;make clean)
	(cd ubistry;make clean)
	(cd ld;make clean)
	(cd edit;make clean)
	(cd ttyd;make clean)
	(cd stat;make clean)
	(cd mount;make clean)
	(cd rtld-elf;make clean)
	(cd tcc;make clean)
