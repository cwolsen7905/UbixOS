# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
#include ../../../Makefile.inc
#include ../../Makefile.inc
#include ../Makefile.inc
include ../libc.inc

#INCLUDES += -I../locale/ -I../i386

#Objects ldexpo
OBJS = __xuname.o _pthread_stubs.o _rand48.o _spinlock_stub.o _thread_init.o alarm.o arc4random.o assert.o basename.o check_utility_compat.o clock.o closedir.o confstr.o crypt.o ctermid.o daemon.o devname.o dirname.o disklabel.o dlfcn.o dlfunc.o drand48.o erand48.o err.o errlst.o errno.o exec.o fmtcheck.o fmtmsg.o fnmatch.o fpclassify.o frexp.o fstab.o ftok.o fts.o ftw.o getbootfile.o getbsize.o getcap.o getcwd.o getdomainname.o getgrent.o getgrouplist.o gethostname.o getloadavg.o getlogin.o getmntinfo.o getnetgrent.o getobjformat.o getosreldate.o getpagesize.o getpeereid.o getprogname.o getpwent.o getttyent.o getusershell.o getvfsbyname.o glob.o initgroups.o isatty.o isinf.o isnan.o jrand48.o lcong48.o lockf.o lrand48.o mrand48.o nftw.o nice.o nlist.o nrand48.o opendir.o pause.o pmadvise.o popen.o posixshm.o pselect.o psignal.o pw_scan.o pwcache.o raise.o readdir.o readpassphrase.o rewinddir.o scandir.o seed48.o seekdir.o sem.o semctl.o setdomainname.o sethostname.o setjmperr.o setmode.o setproctitle.o setprogname.o siginterrupt.o siglist.o signal.o sigsetops.o sleep.o srand48.o statvfs.o stringlist.o strtofflags.o swapcontext.o sysconf.o sysctl.o sysctlbyname.o sysctlnametomib.o syslog.o telldir.o termios.o time.o times.o timezone.o tls.o ttyname.o ttyslot.o ualarm.o ulimit.o uname.o unvis.o usleep.o utime.o valloc.o vis.o wait.o wait3.o waitpid.o wordexp.o

all: $(OBJS)

# Compile the source files
.cc.o:
	$(CXX) $(CFLAGS) $(INCLUDES) -c -o $@ $<
 
.cc.s:
	$(CXX) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

.c.s:
	$(CC) $(CFLAGS) $(INCLUDES) -S -o $@ $<
 
.S.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
# Clean up the junk
clean:
	$(REMOVE) $(OBJS)

