# System Makefile (C) 2002 The UbixOS Project

# Add a directory name here to include it in the build and clean targets.
SUBDIRS = logd authd automountd init login shell clock cp fdisk format disklabel ubistry ttyd ls mount tcc uname cat syscheck make filetest ed ttytest views vlogin ping nc wget aplay mp3play tcsh doom vdoom tessera vi netcfg sndcfg ssltest pwtest rndtest httpsget udptest

.PHONY: all clean ${SUBDIRS}

.for _dir in ${SUBDIRS}
${_dir}:
	(cd ${_dir};${MAKE})
.endfor

all: ${SUBDIRS}

clean:
.for _dir in ${SUBDIRS}
	(cd ${_dir};${MAKE} clean)
.endfor
