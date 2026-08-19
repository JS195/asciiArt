#include "render.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sample.h"

/* Cells beyond this are almost certainly a typo in --width/--height, and the
 * allocation would be measured in gigabytes. */
#define MAX_CELLS 20000000L

int compute_output_dimensions(const AsciiConfig *cfg, int src_w, int src_h,
                              int *out_w, int *out_h) {
    long w, h;

    if (src_w < 1 || src_h < 1) {
        fprintf(stderr, "error: source image has no pixels\n");
        return 1;
    }

    w = cfg->output_width;

    if (cfg->height_explicit) {
        h = cfg->output_height;
    } else {
        double rows = (double)src_h / (double)src_w * (double)w * (double)cfg->char_aspect;
        h = (long)(rows + 0.5);
        if (h < 1) h = 1;
    }

    if (w < 1 || h < 1) {
        fprintf(stderr, "error: computed output size %ldx%ld is empty\n", w, h);
        return 1;
    }
    if (w * h > MAX_CELLS) {
        fprintf(stderr, "error: output %ldx%ld is %ld cells, over the %ld limit\n",
                w, h, w * h, MAX_CELLS);
        return 1;
    }

    *out_w = (int)w;
    *out_h = (int)h;
    return 0;
}

int render_image(const Image *img, const AsciiConfig *cfg, const Charset *cs,
                 AsciiCanvas *canvas) {
    int out_w, out_h, x, y;

    memset(canvas, 0, sizeof(*canvas));

    if (compute_output_dimensions(cfg, img->width, img->height, &out_w, &out_h) != 0) {
        return 1;
    }

    canvas->cells = (Cell *)malloc((size_t)out_w * (size_t)out_h * sizeof(Cell));
    if (canvas->cells == NULL) {
        fprintf(stderr, "error: out of memory allocating a %dx%d cell grid\n", out_w, out_h);
        return 1;
    }
    canvas->width = out_w;
    canvas->height = out_h;

    {
    const Tone tone_params = {
        cfg->levels_black, cfg->levels_white, cfg->gamma, cfg->contrast, cfg->brightness
    };

    for (y = 0; y < out_h; y++) {
        /* Source rect for this row. Integer maths keeps the seams exact: the
         * next row starts where this one ended, with no gaps or overlap. */
        int y0 = (int)(((long)y * img->height) / out_h);
        int y1 = (int)((((long)y + 1) * img->height) / out_h);

        for (x = 0; x < out_w; x++) {
            int x0 = (int)(((long)x * img->width) / out_w);
            int x1 = (int)((((long)x + 1) * img->width) / out_w);
            Cell *cell = &canvas->cells[(size_t)y * (size_t)out_w + (size_t)x];
            RGB avg;
            float tone, ink;

            /* BLOCK RESAMPLE -> AVERAGE RGB (sample_block widens empty rects) */
            avg = sample_block(img->pixels, img->width, img->height, img->channels,
                               x0, y0, x1, y1);

            /* LUMINANCE -> LEVELS/GAMMA/CONTRAST/BRIGHTNESS */
            tone = apply_tone(luminance(avg), &tone_params);

            /* `ink` is how much glyph coverage this cell wants; `tone` stays the
             * source brightness. They are the same by default (light glyphs on a
             * dark background) and opposite under --invert (dark glyphs on a
             * light page), where a dark region must be both the heaviest glyph
             * and a dark foreground -- so only the ink direction flips. */
            ink = cfg->invert ? 255.0f - tone : tone;

            cell->gray_level = -1;

            /* THRESHOLD: too little ink to be worth drawing */
            if (ink < (float)cfg->black_threshold) {
                cell->glyph = ' ';
                cell->color = cfg->fg;
                continue;
            }

            /* CHARACTER SELECTION */
            cell->glyph = charset_glyph(cs, ink / 255.0f);

            /* COLOUR PROCESSING -> QUANTIZATION */
            switch (cfg->mode) {
            case MODE_MONO:
                cell->color = cfg->fg;
                break;
            case MODE_GRAYSCALE: {
                int level = quantize_gray(tone, cfg->gray_levels);
                unsigned char v = gray_level_value(level, cfg->gray_levels);
                cell->gray_level = (short)level;
                cell->color.r = cell->color.g = cell->color.b = v;
                break;
            }
            case MODE_COLOR:
                cell->color = quantize_rgb(adjust_saturation(avg, cfg->saturation),
                                           cfg->color_step);
                break;
            }
        }
    }
    }

    return 0;
}

void canvas_free(AsciiCanvas *canvas) {
    free(canvas->cells);
    memset(canvas, 0, sizeof(*canvas));
}
