# tools/ports/git/config.mak — uBixOS platform definition for git 2.39.5.
#
# build.sh copies this into the git worktree and runs `make uname_S=UbixOS ...`.
# Because config.mak.uname has no `ifeq ($(uname_S),UbixOS)` block, git applies
# ONLY these settings — no glibc-flavoured Linux defaults (NEEDS_LIBRT,
# HAVE_SYNC_FILE_RANGE, /proc/self/exe procinfo, …) to fight.  Everything here is
# an explicit statement about what the uBixOS musl world provides.
#
# CC/AR/CFLAGS/LDFLAGS come from the command line (build.sh) so they win over
# any `:=` in the Makefile.

# ── features we intentionally drop for M0 (local-only git) ───────────────────
NO_CURL      = YesPlease   # no http(s) transport yet (M3)
NO_OPENSSL   = YesPlease   # use git's bundled block-SHA1 + SHA-256 (no libcrypto)
NO_EXPAT     = YesPlease   # only needed for dumb-http push
NO_GETTEXT   = YesPlease   # no message translation
NO_ICONV     = YesPlease   # no charset re-encoding
NO_TCLTK     = YesPlease   # no gitk / git-gui
NO_PERL      = YesPlease   # drops add -i/-p, rebase -i helpers, send-email, svn
NO_PYTHON    = YesPlease   # drops git-p4
NO_GITWEB    = YesPlease
NO_PTHREADS  = YesPlease   # single-threaded: musl threads unproven on-device (M1)
NO_INSTALL_HARDLINKS = YesPlease  # our FS hardlink support is not guaranteed

# ── musl / uBixOS libc capabilities (the honest platform description) ─────────
HAVE_ALLOCA_H          = YesPlease   # musl ships <alloca.h>
HAVE_PATHS_H           = YesPlease   # musl ships <paths.h>
HAVE_DEV_TTY           = YesPlease   # /dev is a real devfs mount
HAVE_GETDELIM          = YesPlease   # musl has getdelim(3)
NO_STRLCPY             = YesPlease   # musl lacks strlcpy/strlcat -> use git compat
NO_REGEX               = NeedsStartEnd   # musl regex lacks REG_STARTEND -> use git's bundled regex
FREAD_READS_DIRECTORIES = UnfortunatelyYes   # fread() on a dir fd succeeds on linux/musl

# Timekeeping: fall back to gettimeofday(2) rather than claim clock_gettime,
# whose CLOCK_MONOTONIC is gated in the -nostdinc world (reference: world
# clock_gettime gotcha).  Leaving HAVE_CLOCK_GETTIME unset is the safe choice.

# Default editor/pager for a console-first system with no $EDITOR/$PAGER set.
DEFAULT_EDITOR = ed
DEFAULT_PAGER  = cat
