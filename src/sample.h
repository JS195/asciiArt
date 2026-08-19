/* sample.h -- block resampling: one output cell = the average of a source rect. */
#ifndef ASCII_SAMPLE_H
#define ASCII_SAMPLE_H

#include "color.h"

/* Averages every pixel in the half-open rect [x0,x1) x [y0,y1).
 *
 * The rect is clamped to the image and is always at least 1x1, so callers can
 * pass a degenerate rect (which happens when the output is wider than the
 * source) without special-casing it.
 *
 * Channel layouts handled: 1 (grey), 2 (grey+alpha), 3 (RGB), >=4 (RGBA, extra
 * channels ignored). Alpha is currently ignored rather than composited. */
RGB sample_block(const unsigned char *img, int width, int height, int channels,
                 int x0, int y0, int x1, int y1);

#endif /* ASCII_SAMPLE_H */
