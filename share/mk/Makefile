NOOBJ=  # defined

.include <ubix.own.mk>

.if ${MKSHARE} != "no"
FILES=  ubix.README ubix.buildinstall.mk ubix.clang-analyze.mk ubix.clean.mk \
        ubix.dep.mk ubix.doc.mk ubix.endian.mk ubix.files.mk ubix.gcc.mk \
        ubix.host.mk ubix.hostlib.mk ubix.hostprog.mk ubix.inc.mk ubix.info.mk \
        ubix.init.mk ubix.ioconf.mk ubix.kernobj.mk ubix.kinc.mk ubix.klinks.mk \
        ubix.kmodule.mk ubix.lib.mk ubix.links.mk ubix.lua.mk \
        ubix.man.mk ubix.nls.mk \
        ubix.obj.mk ubix.own.mk ubix.prog.mk ubix.rpc.mk ubix.shlib.mk \
        ubix.subdir.mk ubix.sys.mk ubix.syscall.mk ubix.test.mk ubix.x11.mk sys.mk

FILESDIR=/usr/share/mk
.endif

FILESMODE=444

.include <ubix.prog.mk>
