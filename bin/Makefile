# System Makefile (C) 2002 The UbixOS Project

# Add a directory name here to include it in the build and clean targets.
#
# aarch64 (Track B port) builds the core CLI world only — the boot triad, the
# coreutils/system tools, and the basic net tools that link just musl + ubix_api.
# The graphics desktop (objgfx: views/vlogin/vdoom) is ported; vdoom also links
# libaudio (virtio-sound on QEMU virt).
# aplay/mp3play now build on aarch64 too (virtio-sound).  Programs needing the
# crypto/net (bearssl/pw/http: ssltest/pwtest/httpsget/wget) library stacks are
# desktop-era and stay i386-only until those libs are ported; clock has a
# bespoke i386 static-link Makefile; the *test asm programs
# (clonetest/tlstest/futextest) are i386-only.
.if ${_ARCH} == "aarch64"
SUBDIRS = logd authd automountd init login shell cp fdisk format disklabel ubistry ubpool ubfs ubdisk diskutil activity files ls mount uname cat syscheck make filetest pipetest ed ttytest ping nc tcsh vi netcfg udptest fbtest views vlogin vdoom tessera aural aplay mp3play tcc basename cut date dirname env find grep head less mkdir more mv ps rm sed sleep sort stat tail touch tr uniq wc which top sh bmake
.else
SUBDIRS = logd authd automountd init login shell clock cp fdisk format disklabel ubistry ubpool ubfs ubdisk diskutil activity files ls mount tcc uname cat syscheck make filetest pipetest vmtest clonetest tlstest futextest pthreadtest condtest detachtest ed ttytest views vlogin ping nc wget aplay mp3play tcsh doom vdoom tessera vi netcfg aural ssltest pwtest rndtest httpsget udptest nsfbtest hello basename cut date dirname env find grep head less mkdir more mv ps rm sed sleep sort stat tail touch tr uniq wc which top sh bmake
.endif

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
