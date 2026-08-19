#include "output.h"

/* Trailing blanks carry no information: only foreground colour is ever set, so
 * a run of spaces at the end of a line is invisible in every format. Trimming
 * them is a large size win on images with dark borders. */
static int line_end(const Cell *row, int width) {
    int end = width;
    while (end > 0 && row[end - 1].glyph == ' ') end--;
    return end;
}

/* The common left margin, trimmed as a block rather than per line.
 *
 * Trailing blanks are already dropped, so the widest line sets the box width.
 * Keeping a shared left margin while trimming the right makes the ink sit
 * off-centre by half that margin -- the box would span columns 0..max, but the
 * picture only occupies margin..max. Removing it makes the emitted block's
 * bounding box equal the ink's, which is what lets a page centre it. */
static int canvas_left_margin(const AsciiCanvas *canvas) {
    int margin = canvas->width;
    int y;

    for (y = 0; y < canvas->height; y++) {
        const Cell *row = &canvas->cells[(size_t)y * (size_t)canvas->width];
        int x = 0;
        while (x < canvas->width && row[x].glyph == ' ') x++;
        if (x < canvas->width && x < margin) margin = x;
    }
    return (margin == canvas->width) ? 0 : margin; /* wholly blank canvas */
}

/* The vertical half of the same crop: the first inked row and one past the last.
 *
 * Blank rows above or below the picture make the emitted block taller than the
 * ink, so a page that aligns something to the block's top edge lands on empty
 * lines instead of on the image. Trimming them is what makes the block's
 * bounding box the ink's box in both axes, not just horizontally. */
static void canvas_ink_rows(const AsciiCanvas *canvas, int *top, int *bottom) {
    int y;

    *top = 0;
    *bottom = 0;
    for (y = 0; y < canvas->height; y++) {
        const Cell *row = &canvas->cells[(size_t)y * (size_t)canvas->width];
        if (line_end(row, canvas->width) == 0) continue; /* row is all spaces */
        if (*bottom == 0) *top = y;
        *bottom = y + 1;
    }
}

static int rgb_equal(RGB a, RGB b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

/* ---------------------------------------------------------------- TXT ---- */

static int write_txt(FILE *f, const AsciiCanvas *canvas) {
    int y, x, top, bottom;
    int left = canvas_left_margin(canvas);
    canvas_ink_rows(canvas, &top, &bottom);
    for (y = top; y < bottom; y++) {
        const Cell *row = &canvas->cells[(size_t)y * (size_t)canvas->width];
        int end = line_end(row, canvas->width);
        for (x = left; x < end; x++) {
            if (fputc(row[x].glyph, f) == EOF) return 1;
        }
        if (fputc('\n', f) == EOF) return 1;
    }
    return 0;
}

/* --------------------------------------------------------------- ANSI ---- */

#define ANSI_RESET "\033[0m"

static int write_ansi(FILE *f, const AsciiCanvas *canvas, const AsciiConfig *cfg) {
    int y, x, top, bottom;
    int left = canvas_left_margin(canvas);

    canvas_ink_rows(canvas, &top, &bottom);
    for (y = top; y < bottom; y++) {
        const Cell *row = &canvas->cells[(size_t)y * (size_t)canvas->width];
        int end = line_end(row, canvas->width);
        RGB current = {0, 0, 0};
        int have_color = 0;

        for (x = left; x < end; x++) {
            const Cell *cell = &row[x];
            RGB want = cell->color;
            int needs_color;

            if (cfg->mode == MODE_MONO) {
                needs_color = cfg->fg_set;
            } else {
                /* A space shows no ink, so never spend an escape on one. */
                needs_color = (cell->glyph != ' ');
            }

            if (needs_color && (!have_color || !rgb_equal(current, want))) {
                if (fprintf(f, "\033[38;2;%u;%u;%um", want.r, want.g, want.b) < 0) return 1;
                current = want;
                have_color = 1;
            }
            if (fputc(cell->glyph, f) == EOF) return 1;
        }

        if (have_color && fputs(ANSI_RESET, f) == EOF) return 1;
        if (fputc('\n', f) == EOF) return 1;
    }
    return 0;
}

/* --------------------------------------------------------------- HTML ---- */

static int html_putc(FILE *f, char c) {
    switch (c) {
    case '&':  return fputs("&amp;", f) == EOF;
    case '<':  return fputs("&lt;", f) == EOF;
    case '>':  return fputs("&gt;", f) == EOF;
    default:   return fputc(c, f) == EOF;
    }
}

static int html_write_css(FILE *f, const AsciiConfig *cfg) {
    /* font-size and line-height come from the same constants as the default
     * --char-aspect (see output.h) -- editing one here without the other is
     * what makes an ASCII render look squashed. */
    if (fprintf(f, "<style>\n"
                   ".ascii-art pre {\n"
                   "    margin: 0;\n"
                   "    white-space: pre;\n"
                   "    font-family: \"JetBrains Mono\", \"SFMono-Regular\", Consolas, monospace;\n"
                   "    font-size: %gpx;\n"
                   "    line-height: %g;\n"
                   "    letter-spacing: 0;\n"
                   "}\n",
                (double)HTML_FONT_SIZE_PX, (double)HTML_LINE_HEIGHT) < 0) {
        return 1;
    }

    if (cfg->mode == MODE_MONO && cfg->fg_set) {
        if (fprintf(f, ".ascii-art pre { color: #%02x%02x%02x; }\n",
                    cfg->fg.r, cfg->fg.g, cfg->fg.b) < 0) {
            return 1;
        }
    }

    if (cfg->mode == MODE_GRAYSCALE) {
        int i;
        for (i = 0; i < cfg->gray_levels; i++) {
            unsigned char v = gray_level_value(i, cfg->gray_levels);
            if (fprintf(f, ".ascii-art .g%d { color: #%02x%02x%02x; }\n", i, v, v, v) < 0) {
                return 1;
            }
        }
    }

    return fputs("</style>\n", f) == EOF;
}

/* Two cells share a span when they need the same foreground. Spaces need none,
 * so they break out of spans and are emitted bare -- that alone removes most of
 * the nodes on any image with a dark background. */
static int same_style(const Cell *a, const Cell *b, OutputMode mode) {
    if (a->glyph == ' ' || b->glyph == ' ') return a->glyph == b->glyph;
    if (mode == MODE_GRAYSCALE) return a->gray_level == b->gray_level;
    return rgb_equal(a->color, b->color);
}

static int html_open_span(FILE *f, const Cell *cell, const AsciiConfig *cfg) {
    if (cfg->mode == MODE_GRAYSCALE) {
        return fprintf(f, "<span class=\"g%d\">", cell->gray_level) < 0;
    }
    return fprintf(f, "<span style=\"color:#%02x%02x%02x\">",
                   cell->color.r, cell->color.g, cell->color.b) < 0;
}

static int write_html(FILE *f, const AsciiCanvas *canvas, const AsciiConfig *cfg) {
    int y, x, top, bottom;
    int left = canvas_left_margin(canvas);

    canvas_ink_rows(canvas, &top, &bottom);

    /* --bare: the host page owns .ascii-art and the grey classes, so emit
     * nothing but the glyph runs. */
    if (!cfg->html_bare) {
        if (html_write_css(f, cfg)) return 1;
        if (fputs("<div class=\"ascii-art\"><pre>", f) == EOF) return 1;
    }

    for (y = top; y < bottom; y++) {
        const Cell *row = &canvas->cells[(size_t)y * (size_t)canvas->width];
        int end = line_end(row, canvas->width);

        /* Newline before each row but the first: a newline just inside the
         * closing </pre> would render as a trailing blank line. */
        if (y > top && fputc('\n', f) == EOF) return 1;

        x = left;
        while (x < end) {
            /* Extend the run over every following cell with the same style. */
            int run = x + 1;
            int styled = (cfg->mode != MODE_MONO) && (row[x].glyph != ' ');
            int i;

            while (run < end && same_style(&row[x], &row[run], cfg->mode)) run++;

            if (styled && html_open_span(f, &row[x], cfg)) return 1;
            for (i = x; i < run; i++) {
                if (html_putc(f, row[i].glyph)) return 1;
            }
            if (styled && fputs("</span>", f) == EOF) return 1;

            x = run;
        }
    }

    if (cfg->html_bare) return 0;
    return fputs("</pre></div>\n", f) == EOF;
}

/* ----------------------------------------------------------- dispatch ---- */

int output_write(FILE *f, const AsciiCanvas *canvas, const AsciiConfig *cfg) {
    int rc;

    switch (cfg->format) {
    case FORMAT_TXT:  rc = write_txt(f, canvas); break;
    case FORMAT_ANSI: rc = write_ansi(f, canvas, cfg); break;
    case FORMAT_HTML: rc = write_html(f, canvas, cfg); break;
    default:          rc = 1; break;
    }

    if (rc != 0 || fflush(f) == EOF || ferror(f)) {
        fprintf(stderr, "error: failed to write output\n");
        return 1;
    }
    return 0;
}
