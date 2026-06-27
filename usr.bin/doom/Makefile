# bin/doom/Makefile — build DOOM (doomgeneric) for UbixOS/i386
# doomgeneric source must be cloned to contrib/doomgeneric/ first:
#   git submodule add https://github.com/ozkl/doomgeneric contrib/doomgeneric

include ../../Makefile.incl
include ../Makefile.incl

DOOM_SRC = ${SRCTOP}/contrib/doomgeneric/doomgeneric
LIBFB_SRC = ${SRCTOP}/lib/libfb

BINARY = doom

.PATH.c: ${DOOM_SRC} ${LIBFB_SRC}

# Discover doomgeneric source files at parse time.  Exclude all platform
# backends (doomgeneric_*.c) and the SDL sound/music files — our UbixOS
# implementations replace those.
DOOM_CSRCS != find ${DOOM_SRC} -maxdepth 1 -name "*.c" \
    ! -name "doomgeneric_*.c" \
    ! -name "i_allegro*.c" \
    ! -name "i_sdl*.c" \
    ! -name "i_sound.c" \
    ! -name "i_music.c" \
    2>/dev/null | sort

DOOM_OBJS  != echo "${DOOM_CSRCS}" | tr ' ' '\n' | \
    sed 's|.*/||;s|\.c$$|.o|' | tr '\n' ' '

OBJS = ${DOOM_OBJS} doomgeneric_ubix.o i_sound_ubix.o i_music_ubix.o opl2.o fb.o font8x8.o

# doomgeneric headers + DOOM compile-time flags.  -w silences the many
# legacy warnings in the DOOM source; -DUBIXOS lets us #ifdef our changes.
EXTRA_CFLAGS = \
    -I${DOOM_SRC} \
    -DNORMALUNIX \
    -DLINUX \
    -DUBIXOS \
    -w \
    -std=gnu99

EXTRA_LIBS = ${OBJ_DIR}/lib/libaudio.a -L${OBJ_DIR}/lib -lubix_api

.include "${UBIX_MK}/ubix.musl.prog.mk"
