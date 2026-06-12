# Application Makefile (C) 2002-2026 The UbixOS Project
#
# aural — the userland sound mixer server (C++).  Talks to /dev/audio directly
# (raw open/ioctl/write with the AUDIO_* numbers from <audio/audio.h>); it does
# NOT link libaudio, whose veneer now routes callers to this server.  ubistry is
# in the base ubix_api link, so no EXTRA_LIBS are needed.

include ../../Makefile.incl
include ../Makefile.incl

BINARY = aural
OBJS   = aural.o aural_server.o audio_sink.o stream_registry.o mixer.o

.include "${UBIX_MK}/ubix.musl.cxx.ubix.prog.mk"
