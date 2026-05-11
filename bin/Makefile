# System Makefile (C) 2002 The UbixOS Project

all: init-bin login-bin shell-bin clock-bin cp-bin fdisk-bin format-bin disklabel-bin ubistry-bin ttyd-bin stat-bin ls-bin tcc-bin uname-bin cat-bin syscheck-bin #muffin-bin
# MrOlsen (2016-01-11) ERROR: weird it didnt like -elf rtld-elf-bin
# MrOlsen (2016-01-11) NOTE: edit-bin
# test-bin pwd-bin de-bin goofball-bin mount-bin

init-bin: init 
	(cd init;$(MAKE))

login-bin: login
	(cd login;$(MAKE))

shell-bin: shell
	(cd shell;$(MAKE))

test-bin: test
	(cd test;$(MAKE))

ls-bin: ls
	(cd ls;$(MAKE))

pwd-bin: pwd
	(cd pwd;$(MAKE))

cat-bin: cat
	(cd cat;$(MAKE))

de-bin: de
	(cd de;$(MAKE))

muffin-bin: muffin
	(cd muffin;$(MAKE))

goofball-bin: goofball
	(cd goofball;$(MAKE))

clock-bin: clock
	(cd clock;$(MAKE))

fdisk-bin: fdisk
	(cd fdisk;$(MAKE))

cp-bin: cp
	(cd cp;$(MAKE))

format-bin: format
	(cd format;$(MAKE))

disklabel-bin: disklabel
	(cd disklabel;$(MAKE))

launcher-bin: launcher
	(cd launcher;$(MAKE))

ubistry-bin: ubistry
	(cd ubistry;$(MAKE))

edit-bin: edit
	(cd edit;$(MAKE))

ttyd-bin: ttyd
	(cd ttyd;$(MAKE))

stat-bin: stat
	(cd stat;$(MAKE))

tcc-bin: tcc
	(cd tcc;$(MAKE))

uname-bin: uname
	(cd uname;$(MAKE))

cat-bin: cat
	(cd cat;$(MAKE))

syscheck-bin: syscheck
	(cd syscheck;$(MAKE))

mount-bin: mount
	(cd mount;$(MAKE))

sh-bin: sh
	(cd sh;$(MAKE))

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
	(cd edit;make clean)
	(cd ttyd;make clean)
	(cd stat;make clean)
	(cd uname;make clean)
	(cd cat;make clean)
	(cd syscheck;make clean)
	(cd mount;make clean)
