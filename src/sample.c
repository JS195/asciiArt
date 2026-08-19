#include "sample.h"

#include <stddef.h>

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

RGB sample_block(const unsigned char *img, int width, int height, int channels,
                 int x0, int y0, int x1, int y1) {
    unsigned long sum_r = 0, sum_g = 0, sum_b = 0, count = 0;
    RGB out = {0, 0, 0};
    int x, y;

    if (img == NULL || width < 1 || height < 1 || channels < 1) return out;

    x0 = clampi(x0, 0, width - 1);
    y0 = clampi(y0, 0, height - 1);
    x1 = clampi(x1, x0 + 1, width);
    y1 = clampi(y1, y0 + 1, height);

    for (y = y0; y < y1; y++) {
        const unsigned char *row = img + ((size_t)y * (size_t)width) * (size_t)channels;
        for (x = x0; x < x1; x++) {
            const unsigned char *p = row + (size_t)x * (size_t)channels;
            if (channels >= 3) {
                sum_r += p[0];
                sum_g += p[1];
                sum_b += p[2];
            } else {
                /* grey and grey+alpha: replicate the single luma channel */
                sum_r += p[0];
                sum_g += p[0];
                sum_b += p[0];
            }
            count++;
        }
    }

    if (count == 0) return out; /* unreachable: the rect is forced to >= 1x1 */

    out.r = (unsigned char)(sum_r / count);
    out.g = (unsigned char)(sum_g / count);
    out.b = (unsigned char)(sum_b / count);
    return out;
}
