# usr.sbin — system daemons + admin services, installed to /usr/sbin
# (C) 2002-2026 The UbixOS Project
#
# FreeBSD/macOS convention: long-running services and their admin tools live
# here (the compositor, the auth/log/automount/registry/audio daemons, the
# graphical login, the ssh server, …).  Add a directory name to SUBDIRS.
SUBDIRS = logd authd automountd ubistry aural netcfg views vlogin

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
