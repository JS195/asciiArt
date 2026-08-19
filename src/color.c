#include "color.h"

#include <math.h>

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static unsigned char clamp_u8(float v) {
    if (v <= 0.0f) return 0;
    if (v >= 255.0f) return 255;
    return (unsigned char)(v + 0.5f);
}

float luminance(RGB c) {
    return 0.299f * (float)c.r + 0.587f * (float)c.g + 0.114f * (float)c.b;
}

float adjust_contrast(float value, float contrast) {
    float normalized = value / 255.0f;
    normalized = (normalized - 0.5f) * contrast + 0.5f;
    return clampf(normalized, 0.0f, 1.0f) * 255.0f;
}

float apply_levels(float value, float black, float white) {
    if (white <= black) return value; /* validated away, but never divide by 0 */
    return clampf((value - black) / (white - black), 0.0f, 1.0f) * 255.0f;
}

float apply_gamma(float value, float gamma) {
    if (gamma == 1.0f) return value;
    return powf(clampf(value / 255.0f, 0.0f, 1.0f), 1.0f / gamma) * 255.0f;
}

float apply_tone(float value, const Tone *t) {
    value = apply_levels(value, t->black, t->white);
    value = apply_gamma(value, t->gamma);
    value = adjust_contrast(value, t->contrast);
    return clampf(value + t->brightness, 0.0f, 255.0f);
}

RGB adjust_saturation(RGB c, float saturation) {
    float y = luminance(c);
    RGB out;
    out.r = clamp_u8(y + ((float)c.r - y) * saturation);
    out.g = clamp_u8(y + ((float)c.g - y) * saturation);
    out.b = clamp_u8(y + ((float)c.b - y) * saturation);
    return out;
}

RGB quantize_rgb(RGB c, int step) {
    RGB out = c;
    if (step > 1) {
        out.r = (unsigned char)((c.r / step) * step);
        out.g = (unsigned char)((c.g / step) * step);
        out.b = (unsigned char)((c.b / step) * step);
    }
    return out;
}

int quantize_gray(float value, int levels) {
    int level;
    if (levels < 2) return 0;
    if (value <= 0.0f) return 0;
    if (value >= 255.0f) return levels - 1;
    level = (int)(value * (float)levels / 256.0f);
    if (level < 0) level = 0;
    if (level > levels - 1) level = levels - 1;
    return level;
}

unsigned char gray_level_value(int level, int levels) {
    if (levels < 2) return 0;
    if (level < 0) level = 0;
    if (level > levels - 1) level = levels - 1;
    return (unsigned char)((level * 255) / (levels - 1));
}
