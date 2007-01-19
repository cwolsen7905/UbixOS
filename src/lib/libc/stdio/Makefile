# $Id$
# The System Makefile (C) 2002 The UbixOS Project

# Include Global 'Source' Options
include ../libc.inc

#INCLUDES += -I../locale/ -I../../../contrib/gdtoa/ -I../i386

#Objects
OBJS = _flock_stub.o asprintf.o clrerr.o fclose.o fdopen.o feof.o ferror.o fflush.o fgetc.o fgetln.o fgetpos.o fgets.o fgetwc.o fgetwln.o fgetws.o fileno.o findfp.o flags.o fopen.o fprintf.o fpurge.o fputc.o fputs.o fputwc.o fputws.o fread.o freopen.o fscanf.o fseek.o fsetpos.o ftell.o funopen.o fvwrite.o fwalk.o fwide.o fwprintf.o fwrite.o fwscanf.o getc.o getchar.o gets.o getw.o getwc.o getwchar.o makebuf.o mktemp.o perror.o printf.o putc.o putchar.o puts.o putw.o putwc.o putwchar.o refill.o remove.o rewind.o rget.o scanf.o setbuf.o setbuffer.o setvbuf.o snprintf.o sprintf.o sscanf.o stdio.o swprintf.o swscanf.o tempnam.o tmpfile.o tmpnam.o ungetc.o ungetwc.o unlocked.o vasprintf.o vfprintf.o vfscanf.o vfwprintf.o vfwscanf.o vprintf.o vscanf.o vsnprintf.o vsprintf.o vsscanf.o vswprintf.o vswscanf.o vwprintf.o vwscanf.o wbuf.o wprintf.o wscanf.o wsetup.o xprintf.o xprintf_errno.o xprintf_float.o xprintf_hexdump.o xprintf_int.o xprintf_quote.o xprintf_str.o xprintf_time.o xprintf_vis.o

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

