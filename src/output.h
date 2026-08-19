/* output.h -- encode a finished canvas as TXT, ANSI or HTML. */
#ifndef ASCII_OUTPUT_H
#define ASCII_OUTPUT_H

#include <stdio.h>

#include "config.h"
#include "render.h"

/* The emitted CSS and the default --char-aspect have to agree or every HTML
 * render comes out stretched. Both are derived from these numbers, so they
 * cannot drift apart. A terminal cell is about twice as tall as it is wide
 * (aspect 0.5), but the tight line-height below makes the HTML cell nearly
 * square -- which is why FORMAT_HTML needs its own default. */
#define HTML_FONT_SIZE_PX  6.0f
#define HTML_LINE_HEIGHT   0.72f
#define HTML_GLYPH_ADVANCE 0.60f /* monospace advance width in em; measured
                                  * 0.60 for JetBrains Mono / SF Mono / Menlo */
#define HTML_CHAR_ASPECT   (HTML_GLYPH_ADVANCE / HTML_LINE_HEIGHT)

/* Returns 0 on success, non-zero on a write error. */
int output_write(FILE *f, const AsciiCanvas *canvas, const AsciiConfig *cfg);

#endif /* ASCII_OUTPUT_H */
