# System Makefile (C) 2002 The UbixOS Project

# Add a directory name here to include it in the build and clean targets.
#
# aarch64 (Track B port) builds the core CLI world only — the boot triad, the
# coreutils/system tools, and the basic net tools that link just musl + ubix_api.
# The graphics desktop (objgfx: views/vlogin/vdoom) is ported; vdoom also links
# libaudio (which runs silent until the virt platform gains a sound device).
# Programs needing the standalone audio tools (aplay/mp3play) or the
# crypto/net (bearssl/pw/http: ssltest/pwtest/httpsget/wget) library stacks are
# desktop-era and stay i386-only until those libs are ported; clock has a
# bespoke i386 static-link Makefile; the *test asm programs
# (clonetest/tlstest/futextest) are i386-only.
.if ${_ARCH} == "aarch64"
SUBDIRS = logd authd automountd init login shell cp fdisk format disklabel ubistry ubpool ubfs ubdisk diskutil activity files ls mount uname cat ln syscheck make filetest pipetest selfhostck ed ttytest ping nc tcsh vi netcfg udptest fbtest views vlogin vdoom tessera aural
.elif ${_ARCH} == "x86_64"
# x86_64 (Track C port) builds the same core CLI + ported-desktop subset as
# aarch64: the boot triad, coreutils/system tools, and the graphics stack, plus the
# network tools wget/httpsget (libhttp+libbearssl are ported and the socket stack
# works — verified end to end with a real HTTP 200 fetch).  The i386-only bits
# (clock/tcc bespoke static links, the *test asm programs) stay out until ported.
SUBDIRS = logd authd automountd init login shell cp fdisk format disklabel ubistry ubpool ubfs ubdisk diskutil activity files ls mount uname cat ln syscheck make filetest pipetest selfhostck ed ttytest ping nc wget httpsget tcsh vi netcfg udptest fbtest views vlogin vdoom tessera aural
.else
SUBDIRS = logd authd automountd init login shell clock cp fdisk format disklabel ubistry ubpool ubfs ubdisk diskutil activity files ls mount tcc uname cat syscheck make filetest pipetest selfhostck vmtest clonetest tlstest futextest pthreadtest condtest detachtest ed ttytest views vlogin ping nc wget aplay mp3play tcsh doom vdoom tessera vi netcfg aural ssltest pwtest rndtest httpsget udptest nsfbtest hello
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
