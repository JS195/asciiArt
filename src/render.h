/* render.h -- image -> grid of ASCII cells.
 *
 * Everything tone- and colour-related is decided here, so the output stage only
 * has to encode an already-final grid. That keeps txt/ansi/html byte-identical
 * in content and makes the pipeline testable without touching a file.
 */
#ifndef ASCII_RENDER_H
#define ASCII_RENDER_H

#include "charset.h"
#include "color.h"
#include "config.h"
#include "image.h"

typedef struct {
    char glyph;      /* ' ' means "blank": below threshold or the ramp's space */
    RGB color;       /* final, quantized foreground (unused in MODE_MONO) */
    short gray_level; /* MODE_GRAYSCALE: 0..gray_levels-1. Otherwise -1. */
} Cell;

typedef struct {
    int width;
    int height;
    Cell *cells; /* width * height, row-major */
} AsciiCanvas;

/* Derives the output grid size. If cfg->height_explicit is unset the height
 * comes from the source aspect ratio corrected by cfg->char_aspect.
 * Returns 0 on success, 1 if the result would be degenerate or too large. */
int compute_output_dimensions(const AsciiConfig *cfg, int src_w, int src_h,
                              int *out_w, int *out_h);

/* Runs the full pipeline. Allocates canvas->cells (free with canvas_free).
 * Returns 0 on success, non-zero on failure (message on stderr). */
int render_image(const Image *img, const AsciiConfig *cfg, const Charset *cs,
                 AsciiCanvas *canvas);

void canvas_free(AsciiCanvas *canvas);

#endif /* ASCII_RENDER_H */
