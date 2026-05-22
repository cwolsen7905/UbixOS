/*
 * opl2.h — Simplified YM3812 (OPL2) software emulator for UbixOS DOOM.
 * GPL-2.0 (lives entirely inside the DOOM binary, not in UbixOS libraries).
 */
#ifndef OPL2_H
#define OPL2_H
#include <stdint.h>

void opl2_reset(uint32_t sample_rate);
void opl2_write(uint8_t reg, uint8_t val);

/*
 * Render n stereo sample pairs into out[] (interleaved L/R int16_t).
 * out[] must have room for 2*n int16_t values.
 * Output is ADDED to the existing buffer contents (for mixing).
 */
void opl2_mix(int16_t *out, int n);

#endif /* OPL2_H */
