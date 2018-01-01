# $FreeBSD: releng/11.1/share/mk/Makefile 296124 2016-02-26 22:14:00Z bdrewery $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

# Only parse this if executing make in this directory, not in other places
# in src that lack a Makefile, such as sys/dev/*.  Otherwise the MAKESYSPATH
# will read this Makefile since it auto includes it into -I.
.if ${.CURDIR} == ${.PARSEDIR}

# Avoid creating a Makefile.depend here as it would get included anywhere
# in the build, similar to the problem above.  It would cause dirdeps.mk
# to be included more times than desired.
UPDATE_DEPENDFILE= no

.include <src.opts.mk>

FILES=	\
	auto.obj.mk \
	ubix.README \
	ubix.arch.inc.mk \
	ubix.clang-analyze.mk \
	ubix.compiler.mk \
	ubix.confs.mk \
	ubix.cpu.mk \
	ubix.crunchgen.mk \
	ubix.dep.mk \
	ubix.doc.mk \
	ubix.dtb.mk \
	ubix.endian.mk \
	ubix.files.mk \
	ubix.incs.mk \
	ubix.info.mk \
	ubix.init.mk \
	ubix.kmod.mk \
	ubix.lib.mk \
	ubix.libnames.mk \
	ubix.links.mk \
	ubix.man.mk \
	ubix.mkopt.mk \
	ubix.nls.mk \
	ubix.obj.mk \
	ubix.opts.mk \
	ubix.own.mk \
	ubix.port.mk \
	ubix.port.options.mk \
	ubix.port.post.mk \
	ubix.port.pre.mk \
	ubix.port.subdir.mk \
	ubix.prog.mk \
	ubix.progs.mk \
	ubix.snmpmod.mk \
	ubix.subdir.mk \
	ubix.symver.mk \
	ubix.sys.mk \
	ubix.test.mk \
	dirdeps.mk \
	gendirdeps.mk \
	install-new.mk \
	meta.autodep.mk \
	meta.stage.mk \
	meta.subdir.mk \
	meta.sys.mk \
	stage-install.sh \
	sys.mk \
	sys.dependfile.mk \
	version_gen.awk

FILESDIR=	${BINDIR}/mk

.if ${MK_TESTS} != "no"
FILES+=	atf.test.mk
FILES+=	plain.test.mk
FILES+=	suite.test.mk
FILES+=	tap.test.mk
.endif

.include <ubix.prog.mk>
.endif	# CURDIR == PARSEDIR
