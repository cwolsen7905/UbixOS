# bin/vdoom/Makefile — windowed DOOM for UbixOS views compositor
# Shares all DOOM engine and audio sources from bin/doom/.
# Only the platform backend differs (doomgeneric_views.c vs doomgeneric_ubix.c).

include ../../Makefile.incl
include ../Makefile.incl

DOOM_SRC  = ${SRCTOP}/contrib/doomgeneric/doomgeneric
DOOM_DIR  = ${SRCTOP}/bin/doom

BINARY = vdoom

.PATH.c: ${DOOM_SRC} ${DOOM_DIR}

DOOM_CSRCS != find ${DOOM_SRC} -maxdepth 1 -name "*.c" \
    ! -name "doomgeneric_*.c" \
    ! -name "i_allegro*.c" \
    ! -name "i_sdl*.c" \
    ! -name "i_sound.c" \
    ! -name "i_music.c" \
    2>/dev/null | sort

DOOM_OBJS != echo "${DOOM_CSRCS}" | tr ' ' '\n' | \
    sed 's|.*/||;s|\.c$$|.o|' | tr '\n' ' '

OBJS = ${DOOM_OBJS} doomgeneric_views.o i_sound_ubix.o i_music_ubix.o opl2.o

EXTRA_CFLAGS = \
    -I${DOOM_SRC} \
    -I${DOOM_DIR} \
    -DNORMALUNIX \
    -DLINUX \
    -DUBIXOS \
    -w \
    -std=gnu99

EXTRA_LIBS = ${OBJ_DIR}/lib/libaudio.a -L${OBJ_DIR}/lib -lubix_api

.include "${UBIX_MK}/ubix.musl.prog.mk"
