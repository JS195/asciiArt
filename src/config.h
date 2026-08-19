/* config.h -- run configuration and command-line parsing. */
#ifndef ASCII_CONFIG_H
#define ASCII_CONFIG_H

#include <stdio.h>

#include "color.h"

#define ASCII_ART_VERSION "1.0.0"

typedef enum {
    MODE_MONO,
    MODE_GRAYSCALE,
    MODE_COLOR
} OutputMode;

typedef enum {
    FORMAT_TXT,
    FORMAT_ANSI,
    FORMAT_HTML
} OutputFormat;

typedef struct {
    int output_width;
    int output_height;
    int height_explicit; /* 0 = derive height from aspect ratio */

    float char_aspect;     /* glyph width:height ratio correction */
    int char_aspect_set;   /* 0 = pick a default to match the output format */

    OutputMode mode;
    OutputFormat format;

    const char *charset;          /* resolved glyph ramp, most ink -> least ink */
    const float *charset_density; /* measured ink coverage, NULL until calibrated */
    const char *charset_name;     /* for messages */

    int invert;

    float contrast;
    float brightness;
    float saturation;

    float levels_black; /* input levels: map [black, white] -> 0..255 */
    float levels_white;
    float gamma;

    int black_threshold;

    int gray_levels;
    int color_step;

    int html_bare; /* html: emit only the glyph markup, no <style>/<pre> */

    RGB fg;     /* mono-mode foreground */
    int fg_set; /* 0 = inherit terminal / page colour, emit no colour at all */

    const char *input_path;
    const char *output_path; /* NULL = stdout */
} AsciiConfig;

void config_defaults(AsciiConfig *cfg);

/* Returns 0 to continue, 1 on a usage error (message on stderr),
 * -1 when --help/--version was handled and the caller should exit 0. */
int config_parse_args(AsciiConfig *cfg, int argc, char **argv);

/* Range checks every field. Returns 0 if valid, 1 otherwise. */
int config_validate(const AsciiConfig *cfg);

void config_print_help(FILE *out, const char *prog);

#endif /* ASCII_CONFIG_H */
