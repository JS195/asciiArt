/* color.h -- colour primitives: luminance, tone curves, saturation, quantization. */
#ifndef ASCII_COLOR_H
#define ASCII_COLOR_H

typedef struct {
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RGB;

/* Rec.601 luma: Y = 0.299R + 0.587G + 0.114B, returned as 0..255 float. */
float luminance(RGB c);

/* Contrast around the 0.5 midpoint. `value` and result are 0..255, clamped. */
float adjust_contrast(float value, float contrast);

/* Input levels: maps [black, white] onto 0..255 and clamps outside it. This is
 * the only control that can expand a narrow input range -- adjust_contrast
 * pivots on the midpoint and clamps, so on a dark image it collapses detail
 * instead of stretching it. */
float apply_levels(float value, float black, float white);

/* Midtone curve. gamma > 1 lifts shadows, < 1 deepens them. 1.0 is a no-op. */
float apply_gamma(float value, float gamma);

/* The tone stage, in pipeline order: levels -> gamma -> contrast -> brightness,
 * clamped to 0..255. Defaults (0/255/1/1/0) make the whole stage a no-op. */
typedef struct {
    float black;      /* input black point, 0..255 */
    float white;      /* input white point, > black */
    float gamma;
    float contrast;
    float brightness; /* offset in luminance units */
} Tone;

float apply_tone(float value, const Tone *t);

/* saturation: 0 = grayscale, 1 = unchanged, >1 = boosted. Clamped per channel. */
RGB adjust_saturation(RGB c, float saturation);

/* Uniform per-channel quantization: v = (v / step) * step. step >= 1. */
RGB quantize_rgb(RGB c, int step);

/* Map 0..255 luminance to a level in 0..levels-1. */
int quantize_gray(float value, int levels);

/* Representative 0..255 grey for a level produced by quantize_gray(). */
unsigned char gray_level_value(int level, int levels);

#endif /* ASCII_COLOR_H */
