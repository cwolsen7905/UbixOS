/*
 * config.h — hand-authored Dropbear autoconf header for the uBixOS musl world.
 *
 * Dropbear's own ./configure runs target test-programs (compile + LINK), which
 * fail under the uBixOS world toolchain (freestanding `aarch64-elf-gcc` +
 * manual musl include/lib paths, no sysroot).  So — exactly like the oksh and
 * bmake ports — we author this from a host ./configure run and adapt it for
 * musl-on-uBixOS.  build.sh copies it over src/config.h before compiling.
 *
 * uBixOS is single-user-session, console-first, no utmp/wtmp/lastlog/syslog/PAM
 * and no BSD libutil — so all of that machinery is disabled here; loginrec.c
 * then compiles to no-ops.  Password auth is routed to authd by a patch (see
 * patches/), not the getspnam/crypt shadow path.
 */
#ifndef DROPBEAR_CONFIG_H_
#define DROPBEAR_CONFIG_H_

/* Bundled libtomcrypt + libtommath (built by build.sh). */
#define BUNDLED_LIBTOM 1

/* --- Disabled subsystems (no uBixOS backing) ----------------------------- */
#define DISABLE_LASTLOG 1
#define DISABLE_PAM 1
#define DISABLE_PUTUTLINE 1
#define DISABLE_PUTUTXLINE 1
#define DISABLE_SYSLOG 1
#define DISABLE_UTMP 1
#define DISABLE_UTMPX 1
#define DISABLE_WTMP 1
#define DISABLE_WTMPX 1
#define DISABLE_ZLIB 1

#define DROPBEAR_FUZZ 0
#define DROPBEAR_PLUGIN 0

/* --- libc functions present in uBixOS musl -------------------------------- */
#define HAVE_BASENAME 1
#define HAVE_CLOCK_GETTIME 1
#define HAVE_CONST_GAI_STRERROR_PROTO 1
#define HAVE_CRYPT 1
#define HAVE_CRYPT_H 1
#define HAVE_DAEMON 1
#define HAVE_EXPLICIT_BZERO 1
#define HAVE_FORK 1
#define HAVE_FREEADDRINFO 1
#define HAVE_GAI_STRERROR 1
#define HAVE_GETADDRINFO 1
#define HAVE_GETGROUPLIST 1
#define HAVE_GETNAMEINFO 1
#define HAVE_GETPASS 1
#define HAVE_GETRANDOM 1
#define HAVE_GETUSERSHELL 1
#define HAVE_OPENPTY 1
#define HAVE_PUTENV 1
#define HAVE_STRLCAT 1
#define HAVE_STRLCPY 1
#define HAVE_WRITEV 1

/* --- libc functions NOT in uBixOS musl (fallbacks used) ------------------- */
/* #undef HAVE_CLEARENV */
/* #undef HAVE_FEXECVE */
/* #undef HAVE_GETSPNAM */          /* shadow path unused (authd patch) */
/* #undef HAVE_MEMSET_S */          /* BSD/C11 annex-K; musl lacks */
/* #undef HAVE_MACH_ABSOLUTE_TIME */
/* #undef HAVE_HTOLE64 */

/* htole64 is provided as a macro by <endian.h> on musl. */
#define HAVE_DECL_HTOLE64 1

/* --- Headers present in uBixOS musl --------------------------------------- */
#define HAVE_ENDIAN_H 1
#define HAVE_INTTYPES_H 1
#define HAVE_LIBGEN_H 1
#define HAVE_NETDB_H 1
#define HAVE_NETINET_IN_H 1
#define HAVE_NETINET_IN_SYSTM_H 1
#define HAVE_NETINET_TCP_H 1
#define HAVE_PATHS_H 1
#define HAVE_PTY_H 1
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRINGS_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_RANDOM_H 1
#define HAVE_SYS_SELECT_H 1
#define HAVE_SYS_SOCKET_H 1
#define HAVE_SYS_STAT_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_UNISTD_H 1

/* --- Headers NOT in uBixOS musl ------------------------------------------- */
/* #undef HAVE_CRYPT_H_BROKEN */
/* #undef HAVE_LASTLOG_H */
/* #undef HAVE_LIBUTIL_H */
/* #undef HAVE_LINUX_PKT_SCHED_H */
/* #undef HAVE_MACH_MACH_TIME_H */
/* #undef HAVE_PAM_PAM_APPL_H */
/* #undef HAVE_SECURITY_PAM_APPL_H */
/* #undef HAVE_SHADOW_H */
/* #undef HAVE_STROPTS_H */
/* #undef HAVE_SYS_ENDIAN_H */
/* #undef HAVE_SYS_PRCTL_H */
/* #undef HAVE_UTIL_H */
/* #undef HAVE_UTMPX_H */
/* #undef HAVE_UTMP_H */

/* --- utmp/utmpx/login machinery: all OFF (no uBixOS backing) -------------- */
/* #undef HAVE_ENDUTENT */
/* #undef HAVE_ENDUTXENT */
/* #undef HAVE_GETUTENT */
/* #undef HAVE_GETUTID */
/* #undef HAVE_GETUTLINE */
/* #undef HAVE_GETUTXENT */
/* #undef HAVE_GETUTXID */
/* #undef HAVE_GETUTXLINE */
/* #undef HAVE_LOGIN */
/* #undef HAVE_LOGOUT */
/* #undef HAVE_LOGWTMP */
/* #undef HAVE_PUTUTLINE */
/* #undef HAVE_PUTUTXLINE */
/* #undef HAVE_SETUTENT */
/* #undef HAVE_SETUTXENT */
/* #undef HAVE_UPDWTMP */
/* #undef HAVE_UTMPNAME */
/* #undef HAVE_UTMPXNAME */

/* --- Types (musl/stdint provide all of these) ----------------------------- */
#define HAVE_UINT8_T 1
#define HAVE_UINT16_T 1
#define HAVE_UINT32_T 1
#define HAVE_U_INT8_T 1
#define HAVE_U_INT16_T 1
#define HAVE_U_INT32_T 1
#define HAVE_STATIC_ASSERT 1
#define HAVE_UNDERSCORE_STATIC_ASSERT 1

#define HAVE_STRUCT_ADDRINFO 1
#define HAVE_STRUCT_IN6_ADDR 1
#define HAVE_STRUCT_SOCKADDR_IN6 1
#define HAVE_STRUCT_SOCKADDR_STORAGE 1
#define HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY 1

/* --- pty: uBixOS uses the /dev/ptmx + openpty() path ---------------------- */
#define USE_DEV_PTMX 1
/* #undef HAVE_DEV_PTS_AND_PTC */
/* #undef HAVE__GETPTY */

/* --- select() arg types --------------------------------------------------- */
#define SELECT_TYPE_ARG1 int
#define SELECT_TYPE_ARG234 (fd_set *)
#define SELECT_TYPE_ARG5 (struct timeval *)

#define STDC_HEADERS 1

/* Package metadata (cosmetic). */
#define PACKAGE_BUGREPORT ""
#define PACKAGE_NAME ""
#define PACKAGE_STRING ""
#define PACKAGE_TARNAME ""
#define PACKAGE_URL ""
#define PACKAGE_VERSION ""

/* GNU extensions: musl gates a number of BSD-isms behind _GNU_SOURCE. */
#define _GNU_SOURCE 1

#endif /* DROPBEAR_CONFIG_H_ */
