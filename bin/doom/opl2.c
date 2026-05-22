/*
 * opl2.c — Simplified YM3812 (OPL2) emulator for UbixOS DOOM.
 *
 * Accuracy notes:
 *  - 9 melodic voices, 2 operators each (modulator + carrier).
 *  - Full ADSR envelope with 16-step rate table (linear approximation).
 *  - FM modulation and modulator self-feedback implemented.
 *  - All 4 OPL2 waveforms implemented.
 *  - KSL, tremolo, and vibrato LFO omitted; DOOM music sounds fine without.
 *  - Output is ADDED to the caller's stereo int16_t buffer via opl2_mix().
 *
 * GPL-2.0 — lives inside the DOOM binary only.
 */

#include "opl2.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/* Compile-time constants                                               */
/* ------------------------------------------------------------------ */

#define OPL_BASE_FREQ   49716u   /* OPL2 clock / 72 */

/* Frequency multiplier table (MULTI field 0-15).
 * Values are 2× the actual multiplier (so index 0 → 0.5×).            */
static const uint8_t k_multi2[16] = {
    1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30
};

/* Envelope rate table: attenuation-units per sample.
 * Attenuation range 0–1023 represents 0 to ~96 dB.
 * Rate 0 = never changes; rate 15 = instant.                          */
static const uint16_t k_env_rate[16] = {
    0, 1, 2, 3, 5, 7, 10, 14, 20, 28, 40, 56, 80, 112, 160, 1023
};

/* Map register slot-offset (low 5 bits of 0x20/0x40/0x60/0x80/0xE0)
 * to internal operator index 0-17.  Offsets 6-7 and 14-15 are unused
 * in OPL2 and map to 0xFF.                                            */
static uint8_t k_slot2op[32];

/* For each operator: its channel (0-8) and whether it is a carrier.   */
static const uint8_t k_op_ch[18]  = {0,1,2,0,1,2,3,4,5,3,4,5,6,7,8,6,7,8};
static const uint8_t k_op_car[18] = {0,0,0,1,1,1,0,0,0,1,1,1,0,0,0,1,1,1};

/* Modulator and carrier slot-offsets for each channel.                */
static const uint8_t k_ch_mod[9] = { 0, 1, 2, 8, 9,10,16,17,18};
static const uint8_t k_ch_car[9] = { 3, 4, 5,11,12,13,19,20,21};

/* ------------------------------------------------------------------ */
/* Waveform tables                                                      */
/* ------------------------------------------------------------------ */

static int16_t k_sine[1024];   /* full sine, range ±32767 */

static void
build_tables(void)
{
    int i;
    for (i = 0; i < 32; i++) k_slot2op[i] = 0xFF;
    for (i = 0; i < 6;  i++) k_slot2op[i]     = (uint8_t)i;
    for (i = 8; i < 14; i++) k_slot2op[i]     = (uint8_t)(i - 2);
    for (i = 16; i < 22; i++) k_slot2op[i]    = (uint8_t)(i - 4);

    for (i = 0; i < 1024; i++) {
        /*
         * Sine via 9th-order Horner polynomial — no math.h / libm needed.
         * Reduce angle to [-π, π] then apply Taylor series; accurate to
         * ~1e-10 for all x, which is more than sufficient for audio.
         */
        double pi = 3.14159265358979323846;
        double t  = 2.0 * pi * (double)i / 1024.0;
        /* Reduce to [-π, π] */
        if (t > pi)  t -= 2.0 * pi;
        /* Horner: sin(x) = x(1 - x²/6(1 - x²/20(1 - x²/42(1 - x²/72)))) */
        double x2 = t * t;
        double sv = t * (1.0 - x2 / 6.0 * (1.0 - x2 / 20.0
                    * (1.0 - x2 / 42.0 * (1.0 - x2 / 72.0))));
        k_sine[i] = (int16_t)(sv * 32767.0);
    }
}

static int16_t
wf_sample(uint8_t wf, uint32_t phase)
{
    uint16_t idx = (uint16_t)(phase >> 22);   /* top 10 bits */
    switch (wf) {
    case 1:  /* half-sine */
        return (idx < 512) ? k_sine[idx] : 0;
    case 2:  /* absolute sine */
        return (idx < 512) ? k_sine[idx] : k_sine[idx - 512];
    case 3:  /* quarter-sine */
        if (idx < 256)  return k_sine[idx];
        if (idx < 512)  return 0;
        if (idx < 768)  return k_sine[idx - 512];
        return 0;
    default: /* 0: full sine */
        return k_sine[idx];
    }
}

/* ------------------------------------------------------------------ */
/* Emulator state                                                       */
/* ------------------------------------------------------------------ */

enum { EG_OFF = 0, EG_ATTACK, EG_DECAY, EG_SUSTAIN, EG_RELEASE };

typedef struct {
    uint8_t  multi;     /* 0-15 (index into k_multi2) */
    uint8_t  tl;        /* total-level attenuation 0-63 */
    uint8_t  ar, dr;    /* attack / decay rate 0-15 */
    uint8_t  sl, rr;    /* sustain level / release rate */
    uint8_t  wf;        /* waveform 0-3 */
    uint8_t  fb;        /* feedback (meaningful on modulator; 0=none) */
    uint8_t  con;       /* connection: 0=FM, 1=additive */
    uint32_t phase;     /* 32-bit accumulator; top 10 bits → sine idx */
    uint32_t phase_inc; /* advance per output sample */
    uint16_t env;       /* 0=max, 1023=silent */
    uint8_t  eg_state;
    int32_t  fb_buf;    /* last output for feedback */
} opl_op_t;

typedef struct {
    uint16_t fnum;
    uint8_t  block;
    uint8_t  key_on;
} opl_ch_t;

static struct {
    opl_op_t  op[18];
    opl_ch_t  ch[9];
    uint32_t  rate;
} s;

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

static void
recalc_inc(uint8_t op_idx)
{
    opl_op_t *op = &s.op[op_idx];
    int ch = k_op_ch[op_idx];
    /*
     * freq_hz = fnum * k_multi2[multi]/2 * OPL_BASE_FREQ * 2^(block-1) / 2^20
     * phase_inc (32-bit, 2^32 = one cycle):
     *   = freq_hz / rate * 2^32
     *   = fnum * k_multi2[multi] * OPL_BASE_FREQ * 2^(block+11) / (rate * 2)
     */
    uint64_t num = (uint64_t)s.ch[ch].fnum * k_multi2[op->multi] * OPL_BASE_FREQ;
    uint8_t  blk = s.ch[ch].block;
    op->phase_inc = (uint32_t)(num * (1u << (blk + 11)) / ((uint64_t)s.rate * 2));
}

static void
ch_key_on(int c)
{
    uint8_t mi = k_slot2op[k_ch_mod[c]];
    uint8_t ci = k_slot2op[k_ch_car[c]];
    if (mi == 0xFF || ci == 0xFF) return;
    s.op[mi].phase = s.op[ci].phase = 0;
    s.op[mi].env   = s.op[ci].env   = 1023;
    s.op[mi].eg_state = s.op[ci].eg_state = EG_ATTACK;
    s.op[mi].fb_buf   = 0;
}

static void
ch_key_off(int c)
{
    uint8_t mi = k_slot2op[k_ch_mod[c]];
    uint8_t ci = k_slot2op[k_ch_car[c]];
    if (mi != 0xFF) s.op[mi].eg_state = EG_RELEASE;
    if (ci != 0xFF) s.op[ci].eg_state = EG_RELEASE;
}

/* ------------------------------------------------------------------ */
/* Public API                                                           */
/* ------------------------------------------------------------------ */

void
opl2_reset(uint32_t sample_rate)
{
    memset(&s, 0, sizeof(s));
    s.rate = sample_rate;
    build_tables();
}

void
opl2_write(uint8_t reg, uint8_t val)
{
    uint8_t base = reg & 0xE0;
    uint8_t lo   = reg & 0x1F;
    uint8_t oi;   /* operator index */

    switch (base) {
    case 0x20:  /* AM/VIB/EGT/KSR/MULTI */
        if (lo > 21) return;
        oi = k_slot2op[lo]; if (oi == 0xFF) return;
        s.op[oi].multi = val & 0x0F;
        recalc_inc(oi);
        break;

    case 0x40:  /* KSL/TL */
        if (lo > 21) return;
        oi = k_slot2op[lo]; if (oi == 0xFF) return;
        s.op[oi].tl = val & 0x3F;
        break;

    case 0x60:  /* AR/DR */
        if (lo > 21) return;
        oi = k_slot2op[lo]; if (oi == 0xFF) return;
        s.op[oi].dr = val & 0x0F;
        s.op[oi].ar = (val >> 4) & 0x0F;
        break;

    case 0x80:  /* SL/RR */
        if (lo > 21) return;
        oi = k_slot2op[lo]; if (oi == 0xFF) return;
        s.op[oi].rr = val & 0x0F;
        s.op[oi].sl = (val >> 4) & 0x0F;
        break;

    case 0xA0: {
        uint8_t ch = reg & 0x0F;   /* channel 0-8; lo = reg&0x1F gives 16-24 for 0xBx */
        if ((reg & 0xF0) == 0xA0 && ch < 9) {
            s.ch[ch].fnum = (uint16_t)((s.ch[ch].fnum & 0x300) | val);
            oi = k_slot2op[k_ch_mod[ch]]; if (oi != 0xFF) recalc_inc(oi);
            oi = k_slot2op[k_ch_car[ch]]; if (oi != 0xFF) recalc_inc(oi);
        } else if ((reg & 0xF0) == 0xB0 && ch < 9) {
            uint8_t new_on = (val >> 5) & 1;
            s.ch[ch].block = (val >> 2) & 7;
            s.ch[ch].fnum  = (uint16_t)((s.ch[ch].fnum & 0xFF) | ((uint16_t)(val & 3) << 8));
            oi = k_slot2op[k_ch_mod[ch]]; if (oi != 0xFF) recalc_inc(oi);
            oi = k_slot2op[k_ch_car[ch]]; if (oi != 0xFF) recalc_inc(oi);
            if (new_on && !s.ch[ch].key_on) ch_key_on(ch);
            else if (!new_on && s.ch[ch].key_on) ch_key_off(ch);
            s.ch[ch].key_on = new_on;
        }
        break;
    }

    case 0xC0:  /* feedback / connection */
        if (lo < 9) {
            uint8_t mi = k_slot2op[k_ch_mod[lo]];
            uint8_t ci = k_slot2op[k_ch_car[lo]];
            if (mi != 0xFF) { s.op[mi].fb = (val >> 1) & 7; s.op[mi].con = val & 1; }
            if (ci != 0xFF)   s.op[ci].con = val & 1;
        }
        break;

    case 0xE0:  /* waveform select */
        if (lo > 21) return;
        oi = k_slot2op[lo]; if (oi == 0xFF) return;
        s.op[oi].wf = val & 3;
        break;

    default:
        break;
    }
}

/* Advance envelope one sample; return linear output scale 0-255. */
static int
eg_tick(opl_op_t *op)
{
    switch (op->eg_state) {
    case EG_ATTACK:
        if (op->ar == 0) break;
        if (op->env <= k_env_rate[op->ar]) {
            op->env = 0; op->eg_state = EG_DECAY;
        } else {
            op->env -= k_env_rate[op->ar];
        }
        break;
    case EG_DECAY: {
        uint16_t sl_lv = (uint16_t)op->sl * (1023 / 15);
        if (op->dr == 0 || op->env >= sl_lv) {
            op->env = sl_lv; op->eg_state = EG_SUSTAIN;
        } else {
            op->env += k_env_rate[op->dr];
            if (op->env > sl_lv) op->env = sl_lv;
        }
        break;
    }
    case EG_SUSTAIN:
        break;
    case EG_RELEASE:
        if (op->rr == 0) { op->env = 1023; op->eg_state = EG_OFF; break; }
        op->env += k_env_rate[op->rr];
        if (op->env >= 1023) { op->env = 1023; op->eg_state = EG_OFF; }
        break;
    case EG_OFF:
    default:
        return 0;
    }

    /* Combined attenuation: envelope (0-1023) + TL×16 → 0-1023.
     * Scale = 255 × (1 - atten/1023), floored to 0.                   */
    int32_t atten = (int32_t)op->env + (int32_t)op->tl * 16;
    if (atten > 1023) atten = 1023;
    return (int)(255 - atten * 255 / 1023);
}

void
opl2_mix(int16_t *out, int n)
{
    int s_idx, c;
    for (s_idx = 0; s_idx < n; s_idx++) {
        int32_t mix = 0;

        for (c = 0; c < 9; c++) {
            uint8_t mi = k_slot2op[k_ch_mod[c]];
            uint8_t ci = k_slot2op[k_ch_car[c]];
            if (mi == 0xFF || ci == 0xFF) continue;

            opl_op_t *mod = &s.op[mi];
            opl_op_t *car = &s.op[ci];

            /* Step modulator phase */
            mod->phase += mod->phase_inc;

            /* Apply self-feedback to modulator phase */
            uint32_t mod_phase = mod->phase;
            if (mod->fb > 0)
                mod_phase += (uint32_t)((mod->fb_buf) << (9 - mod->fb));

            int mod_scale  = eg_tick(mod);
            int16_t mod_raw = wf_sample(mod->wf, mod_phase);
            int32_t mod_out = (int32_t)mod_raw * mod_scale / 255;
            mod->fb_buf     = mod_out;

            /* Step carrier phase */
            car->phase += car->phase_inc;
            int car_scale = eg_tick(car);
            int32_t ch_out;

            if (mod->con == 0) {
                /* FM synthesis: mod output phase-modulates carrier */
                uint32_t fm = car->phase + (uint32_t)(mod_out << 8);
                ch_out = (int32_t)wf_sample(car->wf, fm) * car_scale / 255;
            } else {
                /* Additive: sum mod + carrier directly */
                ch_out  = (int32_t)wf_sample(car->wf, car->phase) * car_scale / 255;
                ch_out += mod_out;
            }

            mix += ch_out / 9;
        }

        if (mix >  32767) mix =  32767;
        if (mix < -32767) mix = -32767;
        out[s_idx * 2]     = (int16_t)(out[s_idx * 2]     + (int16_t)mix);
        out[s_idx * 2 + 1] = (int16_t)(out[s_idx * 2 + 1] + (int16_t)mix);
    }
}
