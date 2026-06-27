/*
 * Copyright (c) 2002-2026 The UbixOS Project.
 * Distributed under the GNU General Public License v2.
 *
 * i_music_ubix.c — MUS sequencer → OPL2 → PCM music for UbixOS DOOM.
 *
 * Architecture:
 *   I_InitMusic    — load GENMIDI lump; initialise OPL2 emulator.
 *   I_RegisterSong — copy MUS data, record score start offset.
 *   I_PlaySong     — arm the sequencer for playback.
 *   I_MusicMix     — called each game tick by i_sound_ubix.c; advances
 *                    the MUS event timeline and mixes OPL2 PCM output
 *                    into the caller's stereo int16_t buffer.
 *
 * MUS timing: 140 ticks/sec (DOOM default).
 * OPL2 voices: 9 melodic, multiplexed from up to 16 MUS channels.
 */

#include "doomtype.h"
#include "i_sound.h"
#include "w_wad.h"
#include "z_zone.h"
#include "opl2.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ------------------------------------------------------------------ */
/* Sound-device globals required by doomgeneric s_sound.c              */
/* ------------------------------------------------------------------ */

int  snd_sfxdevice       = 3;    /* SNDDEVICE_SB — enables digital SFX   */
int  snd_musicdevice     = 3;    /* SNDDEVICE_SB — enables I_RegisterSong */
int  snd_samplerate      = 44100;
int  snd_cachesize       = 64 * 1024;
int  snd_maxslicetime_ms = 28;
char *snd_musiccmd        = "";

void I_BindSoundVariables(void) {}

/* ------------------------------------------------------------------ */
/* MUS format constants                                                 */
/* ------------------------------------------------------------------ */

#define MUS_MAGIC        "\x4d\x55\x53\x1a"   /* "MUS\x1a" */
#define MUS_TICKS_PS     140u                  /* MUS ticks per second */
#define MUS_NOTEOFF      0
#define MUS_NOTEON       1
#define MUS_PITCHWHEEL   2
#define MUS_SYSEVENT     3
#define MUS_CTRLCHANGE   4
#define MUS_SCOREEND     6

typedef struct {
    uint8_t  id[4];
    uint16_t score_len;
    uint16_t score_start;
    uint16_t primary_channels;
    uint16_t secondary_channels;
    uint16_t instrument_count;
    uint16_t dummy;
} __attribute__((packed)) mus_hdr_t;

/* ------------------------------------------------------------------ */
/* GENMIDI lump constants                                               */
/* ------------------------------------------------------------------ */

#define GENMIDI_MAGIC     "#OPL_II#"
#define GENMIDI_INSTR_SZ  36
#define GENMIDI_NUM_MEL   128

/*
 * Voice layout within a 36-byte GENMIDI entry (voice 0 at byte 4,
 * voice 1 at byte 18; each voice is 14 bytes):
 *   [0] mod: am/vib/egt/ksr/multi
 *   [1] mod: ksl/tl
 *   [2] mod: ar/dr
 *   [3] mod: sl/rr
 *   [4] mod: waveform
 *   [5] car: am/vib/egt/ksr/multi
 *   [6] car: ksl/tl
 *   [7] car: ar/dr
 *   [8] car: sl/rr
 *   [9] car: waveform
 *  [10] feedback | connection
 *  [11] unused
 *  [12-13] base note offset (int16_t LE)
 */
#define GV_OFF_VOICE0    4
#define GV_MOD_AMVEG     0
#define GV_MOD_KSLTL     1
#define GV_MOD_ARDR      2
#define GV_MOD_SLRR      3
#define GV_MOD_WAVE      4
#define GV_CAR_AMVEG     5
#define GV_CAR_KSLTL     6
#define GV_CAR_ARDR      7
#define GV_CAR_SLRR      8
#define GV_CAR_WAVE      9
#define GV_FBCON        10

/* OPL2 operator slot-offsets for each of the 9 channels               */
static const uint8_t k_mod_slot[9] = { 0, 1, 2, 8, 9,10,16,17,18};
static const uint8_t k_car_slot[9] = { 3, 4, 5,11,12,13,19,20,21};

/* ------------------------------------------------------------------ */
/* State                                                                */
/* ------------------------------------------------------------------ */

/* GENMIDI lump pointer (into WAD cache) */
static const uint8_t *g_genmidi;
static int            g_genmidi_ok;

/* Voice pool */
#define NUM_VOICES 9

typedef struct {
    int      in_use;
    uint8_t  mus_ch;    /* owning MUS channel */
    uint8_t  note;      /* MIDI note number */
    uint8_t  block;     /* OPL2 block (needed for key-off) */
    uint8_t  fnum_hi;   /* fnum bits [9:8] (needed for key-off) */
    uint32_t age;       /* monotonic counter for LRU stealing */
} voice_t;

static voice_t  g_voice[NUM_VOICES];
static uint32_t g_voice_tick;

/* Per-MUS-channel state */
static uint8_t g_ch_prog[16];      /* current instrument number */
static uint8_t g_ch_vol[16];       /* channel volume (ctrl 3) */
static uint8_t g_ch_last_vol[16];  /* last note-on velocity */

/* MUS playback state */
static uint8_t  *g_mus;            /* malloc'd copy of song data */
static int       g_mus_len;
static int       g_mus_pos;        /* byte offset into score */
static int       g_mus_looping;
static int       g_mus_playing;
static uint32_t  g_tick_wait;      /* MUS ticks until next event */

/* ------------------------------------------------------------------ */
/* Frequency helper                                                     */
/* ------------------------------------------------------------------ */

/*
 * Convert MIDI note to OPL2 (fnum, block).
 * Uses the standard OPL2 equal-temperament table centred at A4=440 Hz.
 * OPL2 frequency: f = fnum * OPL_BASE_FREQ * 2^(block-1) / 2^20.
 */
static void
note_to_fb(uint8_t note, uint16_t *fnum, uint8_t *block)
{
    /*
     * fnum_base[pc] gives the F-number for each pitch class at block 4.
     * Values computed as: round(440 * 2^((pc-9)/12) * 2^20 / (49716 * 8)).
     */
    static const uint16_t base[12] = {
        343, 364, 385, 408, 433, 458, 485, 514, 544, 577, 611, 647
    };
    int octave = (int)(note / 12) - 1;
    int pc     = (int)(note % 12);
    if (octave < 0) octave = 0;
    if (octave > 7) octave = 7;
    *fnum  = base[pc];
    *block = (uint8_t)octave;
}

/* ------------------------------------------------------------------ */
/* OPL2 voice programming                                               */
/* ------------------------------------------------------------------ */

static void
prog_voice(int v, uint8_t instr, uint8_t vol)
{
    const uint8_t *e;
    const uint8_t *vd;
    uint8_t ms, cs;
    uint8_t tl_adjust, car_tl, mod_tl;

    if (!g_genmidi_ok) return;
    if (instr >= GENMIDI_NUM_MEL) instr = 0;

    e  = g_genmidi + 8 + (int)instr * GENMIDI_INSTR_SZ;
    vd = e + GV_OFF_VOICE0;
    ms = k_mod_slot[v];
    cs = k_car_slot[v];

    /*
     * Carrier TL: instrument TL blended with note volume.
     * vol 127 → no extra attenuation; vol 0 → add 63 (silence).
     */
    tl_adjust = (uint8_t)(63 - (int)vol * 63 / 127);
    car_tl    = (uint8_t)((vd[GV_CAR_KSLTL] & 0x3F) + tl_adjust);
    if (car_tl > 63) car_tl = 63;
    mod_tl    = vd[GV_MOD_KSLTL] & 0x3F;

    /* Modulator */
    opl2_write(0x20 + ms, vd[GV_MOD_AMVEG]);
    opl2_write(0x40 + ms, (uint8_t)((vd[GV_MOD_KSLTL] & 0xC0) | mod_tl));
    opl2_write(0x60 + ms, vd[GV_MOD_ARDR]);
    opl2_write(0x80 + ms, vd[GV_MOD_SLRR]);
    opl2_write(0xE0 + ms, vd[GV_MOD_WAVE] & 3);

    /* Carrier */
    opl2_write(0x20 + cs, vd[GV_CAR_AMVEG]);
    opl2_write(0x40 + cs, (uint8_t)((vd[GV_CAR_KSLTL] & 0xC0) | car_tl));
    opl2_write(0x60 + cs, vd[GV_CAR_ARDR]);
    opl2_write(0x80 + cs, vd[GV_CAR_SLRR]);
    opl2_write(0xE0 + cs, vd[GV_CAR_WAVE] & 3);

    /* Feedback / connection */
    opl2_write(0xC0 + v, vd[GV_FBCON]);
}

static void
voice_on(int v, uint8_t note, uint8_t instr, uint8_t vol)
{
    uint16_t fnum;
    uint8_t  block;

    /* Key-off before reprogramming */
    opl2_write(0xB0 + v,
               (uint8_t)((g_voice[v].block << 2) | g_voice[v].fnum_hi));

    note_to_fb(note, &fnum, &block);
    g_voice[v].block   = block;
    g_voice[v].fnum_hi = (uint8_t)(fnum >> 8) & 3;

    prog_voice(v, instr, vol);
    opl2_write(0xA0 + v, (uint8_t)(fnum & 0xFF));
    opl2_write(0xB0 + v,
               (uint8_t)(0x20 | (block << 2) | ((fnum >> 8) & 3)));
}

static void
voice_off(int v)
{
    /* Clear key-on bit; preserve block and fnum for silence settling. */
    opl2_write(0xB0 + v,
               (uint8_t)((g_voice[v].block << 2) | g_voice[v].fnum_hi));
    g_voice[v].in_use = 0;
}

/* ------------------------------------------------------------------ */
/* Voice allocator                                                      */
/* ------------------------------------------------------------------ */

static int
alloc_voice(uint8_t ch, uint8_t note)
{
    int i, oldest;
    uint32_t min_age;

    for (i = 0; i < NUM_VOICES; i++) {
        if (!g_voice[i].in_use) {
            g_voice[i].in_use  = 1;
            g_voice[i].mus_ch  = ch;
            g_voice[i].note    = note;
            g_voice[i].age     = g_voice_tick;
            return i;
        }
    }
    /* Steal oldest voice (LRU) */
    oldest = 0; min_age = g_voice[0].age;
    for (i = 1; i < NUM_VOICES; i++) {
        if (g_voice[i].age < min_age) { min_age = g_voice[i].age; oldest = i; }
    }
    voice_off(oldest);
    g_voice[oldest].in_use = 1;
    g_voice[oldest].mus_ch = ch;
    g_voice[oldest].note   = note;
    g_voice[oldest].age    = g_voice_tick;
    return oldest;
}

static void
release_voice(uint8_t ch, uint8_t note)
{
    int i;
    for (i = 0; i < NUM_VOICES; i++) {
        if (g_voice[i].in_use && g_voice[i].mus_ch == ch
                && g_voice[i].note == note) {
            voice_off(i);
            return;
        }
    }
}

/* ------------------------------------------------------------------ */
/* MUS event parsing                                                    */
/* ------------------------------------------------------------------ */

static uint8_t
mus_byte(void)
{
    if (g_mus_pos >= g_mus_len) { g_mus_playing = 0; return 0; }
    return g_mus[g_mus_pos++];
}

static uint32_t
mus_delta(void)
{
    uint32_t t = 0;
    uint8_t  b;
    do { b = mus_byte(); t = t * 128 + (b & 0x7F); } while (b & 0x80);
    return t;
}

/*
 * Process MUS events until the next event with a non-zero delta time.
 * Sets g_tick_wait to the number of MUS ticks to pause before the
 * following event.
 */
static void
mus_pump(void)
{
    while (g_mus_playing && g_tick_wait == 0) {
        uint8_t ev   = mus_byte();
        uint8_t type = (ev >> 4) & 0x7;
        uint8_t ch   = ev & 0xF;
        int     last = (ev >> 7) & 1;

        switch (type) {
        case MUS_NOTEON: {
            uint8_t b    = mus_byte();
            uint8_t note = b & 0x7F;
            uint8_t vel;
            if (b & 0x80) { vel = mus_byte() & 0x7F; g_ch_last_vol[ch] = vel; }
            else             vel = g_ch_last_vol[ch];
            if (ch != 15) { /* skip percussion */
                uint8_t eff = (uint8_t)((int)vel * g_ch_vol[ch] / 127);
                int v = alloc_voice(ch, note);
                voice_on(v, note, g_ch_prog[ch], eff);
            }
            break;
        }
        case MUS_NOTEOFF:
            if (ch != 15) release_voice(ch, mus_byte() & 0x7F);
            else mus_byte();
            break;

        case MUS_PITCHWHEEL:
            mus_byte(); /* ignore */
            break;

        case MUS_SYSEVENT:
            mus_byte(); /* ignore */
            break;

        case MUS_CTRLCHANGE: {
            uint8_t ctrl = mus_byte();
            uint8_t cval = mus_byte();
            if (ctrl == 0)  g_ch_prog[ch] = cval & 0x7F;   /* patch */
            if (ctrl == 3)  g_ch_vol[ch]  = cval & 0x7F;   /* volume */
            break;
        }

        case MUS_SCOREEND:
            if (g_mus_looping) {
                mus_hdr_t *hdr = (mus_hdr_t *)g_mus;
                g_mus_pos = hdr->score_start;
                g_tick_wait = 0;
            } else {
                g_mus_playing = 0;
            }
            return;

        default:
            break;
        }

        if (last) g_tick_wait = mus_delta();
    }
}

/* ------------------------------------------------------------------ */
/* I_MusicMix — called each game tick by i_sound_ubix.c               */
/* ------------------------------------------------------------------ */

void
I_MusicMix(int16_t *buf, int n)
{
    int     out = 0;
    uint32_t spmt;    /* samples per MUS tick */

    if (!g_mus_playing || !g_genmidi_ok) return;

    g_voice_tick++;
    spmt = (uint32_t)snd_samplerate / MUS_TICKS_PS; /* ≈ 315 at 44100 Hz */
    if (spmt == 0) spmt = 1;

    while (out < n) {
        /* Fire any events that are due */
        if (g_tick_wait == 0) {
            mus_pump();
            if (!g_mus_playing) break;
        }

        /* Render up to the next event boundary */
        int chunk = n - out;
        uint32_t samples_ahead = g_tick_wait * spmt;
        if ((uint32_t)chunk > samples_ahead) chunk = (int)samples_ahead;
        if (chunk <= 0) chunk = 1;

        opl2_mix(buf + out * 2, chunk);
        out += chunk;

        /* Advance the tick counter */
        uint32_t ticks_used = (uint32_t)chunk / spmt;
        if (ticks_used >= g_tick_wait) g_tick_wait = 0;
        else                           g_tick_wait -= ticks_used;
    }
}

/* ------------------------------------------------------------------ */
/* Standard DOOM I_* music interface                                    */
/* ------------------------------------------------------------------ */

void
I_InitMusic(void)
{
    int lump;

    opl2_reset((uint32_t)snd_samplerate);
    memset(g_voice,        0, sizeof(g_voice));
    memset(g_ch_prog,      0, sizeof(g_ch_prog));
    memset(g_ch_vol,     127, sizeof(g_ch_vol));
    memset(g_ch_last_vol,  64, sizeof(g_ch_last_vol));

    /* Enable OPL2 waveform select (register 0x01 bit 5) */
    opl2_write(0x01, 0x20);

    lump = W_CheckNumForName("GENMIDI");
    if (lump < 0) {
        fprintf(stderr, "I_InitMusic: GENMIDI lump missing — no music\n");
        fflush(stderr);
        return;
    }
    g_genmidi    = (const uint8_t *)W_CacheLumpNum(lump, PU_STATIC);
    g_genmidi_ok = (memcmp(g_genmidi, GENMIDI_MAGIC, 8) == 0);
    fprintf(stderr, "I_InitMusic: GENMIDI %s\n",
            g_genmidi_ok ? "OK" : "bad magic — no music");
    fflush(stderr);
}

void
I_ShutdownMusic(void)
{
    g_mus_playing = 0;
    free(g_mus); g_mus = NULL;
}

void I_SetMusicVolume(int v) { (void)v; }
void I_PauseSong(void)       { g_mus_playing = 0; }
void I_ResumeSong(void)      { if (g_mus) g_mus_playing = 1; }

void *
I_RegisterSong(void *data, int len)
{
    mus_hdr_t *hdr;

    if (!g_genmidi_ok || len < (int)sizeof(mus_hdr_t)) return NULL;
    hdr = (mus_hdr_t *)data;
    if (memcmp(hdr->id, MUS_MAGIC, 4) != 0) {
        fprintf(stderr, "I_RegisterSong: not a MUS file\n"); fflush(stderr);
        return NULL;
    }

    free(g_mus);
    g_mus = (uint8_t *)malloc((size_t)len);
    if (!g_mus) return NULL;
    memcpy(g_mus, data, (size_t)len);
    g_mus_len     = len;
    g_mus_playing = 0;
    g_mus_pos     = 0;
    g_tick_wait   = 0;

    fprintf(stderr, "I_RegisterSong: score_start=%u\n", hdr->score_start);
    fflush(stderr);
    return g_mus;
}

void
I_PlaySong(void *handle, boolean looping)
{
    mus_hdr_t *hdr;
    int i;

    if (!handle || !g_genmidi_ok) return;

    /* Key-off all voices from previous song */
    for (i = 0; i < NUM_VOICES; i++)
        if (g_voice[i].in_use) { voice_off(i); }

    hdr           = (mus_hdr_t *)g_mus;
    g_mus_pos     = hdr->score_start;
    g_tick_wait   = 0;
    g_mus_looping = looping ? 1 : 0;
    g_mus_playing = 1;
    mus_pump();
}

void
I_StopSong(void)
{
    int i;
    g_mus_playing = 0;
    for (i = 0; i < NUM_VOICES; i++)
        if (g_voice[i].in_use) voice_off(i);
}

void
I_UnRegisterSong(void *handle)
{
    (void)handle;
    I_StopSong();
    free(g_mus); g_mus = NULL;
}

boolean
I_MusicIsPlaying(void)
{
    return (boolean)g_mus_playing;
}
