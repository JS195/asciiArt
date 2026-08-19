#include "config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "charset.h"

void config_defaults(AsciiConfig *cfg) {
    Charset cs;

    memset(cfg, 0, sizeof(*cfg));

    cfg->output_width = 100;
    cfg->output_height = 0;
    cfg->height_explicit = 0;
    cfg->char_aspect = 0.5f; /* terminal cell; HTML overrides in main */
    cfg->char_aspect_set = 0;
    cfg->mode = MODE_MONO;
    cfg->format = FORMAT_TXT;
    cfg->invert = 0;
    cfg->contrast = 1.0f;
    cfg->brightness = 0.0f;
    cfg->saturation = 1.0f;
    cfg->levels_black = 0.0f;
    cfg->levels_white = 255.0f;
    cfg->gamma = 1.0f;
    cfg->black_threshold = 0;
    cfg->gray_levels = 16;
    cfg->color_step = 16;
    cfg->fg.r = cfg->fg.g = cfg->fg.b = 255;
    cfg->fg_set = 0;
    cfg->input_path = NULL;
    cfg->output_path = NULL;

    charset_lookup(&cs, "medium");
    cfg->charset = cs.glyphs;
    cfg->charset_density = cs.density;
    cfg->charset_name = cs.name;
}

static int parse_int(const char *opt, const char *s, int *out) {
    char *end;
    long v;
    errno = 0;
    v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || errno == ERANGE || v < -2147483647L || v > 2147483647L) {
        fprintf(stderr, "error: %s expects an integer, got '%s'\n", opt, s);
        return 1;
    }
    *out = (int)v;
    return 0;
}

static int parse_float(const char *opt, const char *s, float *out) {
    char *end;
    double v;
    errno = 0;
    v = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE) {
        fprintf(stderr, "error: %s expects a number, got '%s'\n", opt, s);
        return 1;
    }
    *out = (float)v;
    return 0;
}

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int parse_color(const char *opt, const char *s, RGB *out) {
    int v[6];
    int i;
    if (*s == '#') s++;
    if (strlen(s) != 6) goto bad;
    for (i = 0; i < 6; i++) {
        v[i] = hex_nibble(s[i]);
        if (v[i] < 0) goto bad;
    }
    out->r = (unsigned char)(v[0] * 16 + v[1]);
    out->g = (unsigned char)(v[2] * 16 + v[3]);
    out->b = (unsigned char)(v[4] * 16 + v[5]);
    return 0;
bad:
    fprintf(stderr, "error: %s expects a hex colour like #ff8800, got '%s'\n", opt, s);
    return 1;
}

/* "LO,HI" -- the input range to stretch onto the full output range. */
static int parse_levels(const char *opt, const char *s, float *lo, float *hi) {
    char *end;
    double a, b;
    errno = 0;
    a = strtod(s, &end);
    if (end == s || *end != ',' || errno == ERANGE) goto bad;
    s = end + 1;
    errno = 0;
    b = strtod(s, &end);
    if (end == s || *end != '\0' || errno == ERANGE) goto bad;
    *lo = (float)a;
    *hi = (float)b;
    return 0;
bad:
    fprintf(stderr, "error: %s expects LO,HI (e.g. 18,130), got '%s'\n", opt, s);
    return 1;
}

static int parse_mode(const char *s, OutputMode *out) {
    if (strcmp(s, "mono") == 0) { *out = MODE_MONO; return 0; }
    if (strcmp(s, "gray") == 0 || strcmp(s, "grey") == 0 ||
        strcmp(s, "grayscale") == 0 || strcmp(s, "greyscale") == 0) {
        *out = MODE_GRAYSCALE;
        return 0;
    }
    if (strcmp(s, "color") == 0 || strcmp(s, "colour") == 0) { *out = MODE_COLOR; return 0; }
    fprintf(stderr, "error: --mode expects mono, gray or color, got '%s'\n", s);
    return 1;
}

static int parse_format(const char *s, OutputFormat *out) {
    if (strcmp(s, "txt") == 0 || strcmp(s, "text") == 0) { *out = FORMAT_TXT; return 0; }
    if (strcmp(s, "ansi") == 0) { *out = FORMAT_ANSI; return 0; }
    if (strcmp(s, "html") == 0) { *out = FORMAT_HTML; return 0; }
    fprintf(stderr, "error: --format expects txt, ansi or html, got '%s'\n", s);
    return 1;
}

/* Consumes the value for an option that requires one. */
#define NEED_VALUE(opt)                                                      \
    do {                                                                     \
        if (i + 1 >= argc) {                                                 \
            fprintf(stderr, "error: %s requires a value\n", (opt));           \
            return 1;                                                        \
        }                                                                    \
        value = argv[++i];                                                   \
    } while (0)

int config_parse_args(AsciiConfig *cfg, int argc, char **argv) {
    int i;
    const char *prog = (argc > 0 && argv[0] != NULL) ? argv[0] : "ascii-art";

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value;

        if (strcmp(arg, "--help") == 0 || strcmp(arg, "-h") == 0) {
            config_print_help(stdout, prog);
            return -1;
        }
        if (strcmp(arg, "--version") == 0) {
            printf("ascii-art %s\n", ASCII_ART_VERSION);
            return -1;
        }
        if (strcmp(arg, "--mode") == 0) {
            NEED_VALUE("--mode");
            if (parse_mode(value, &cfg->mode)) return 1;
        } else if (strcmp(arg, "--format") == 0) {
            NEED_VALUE("--format");
            if (parse_format(value, &cfg->format)) return 1;
        } else if (strcmp(arg, "--width") == 0) {
            NEED_VALUE("--width");
            if (parse_int("--width", value, &cfg->output_width)) return 1;
        } else if (strcmp(arg, "--height") == 0) {
            NEED_VALUE("--height");
            if (parse_int("--height", value, &cfg->output_height)) return 1;
            cfg->height_explicit = 1;
        } else if (strcmp(arg, "--char-aspect") == 0) {
            NEED_VALUE("--char-aspect");
            if (parse_float("--char-aspect", value, &cfg->char_aspect)) return 1;
            cfg->char_aspect_set = 1;
        } else if (strcmp(arg, "--charset") == 0) {
            Charset cs;
            NEED_VALUE("--charset");
            if (charset_lookup(&cs, value) != 0) {
                fprintf(stderr, "error: unknown charset '%s' (available: %s)\n",
                        value, charset_names());
                return 1;
            }
            cfg->charset = cs.glyphs;
            cfg->charset_density = cs.density;
            cfg->charset_name = cs.name;
        } else if (strcmp(arg, "--charset-custom") == 0) {
            NEED_VALUE("--charset-custom");
            cfg->charset = value;
            cfg->charset_density = NULL;
            cfg->charset_name = "custom";
        } else if (strcmp(arg, "--bare") == 0) {
            cfg->html_bare = 1;
        } else if (strcmp(arg, "--invert") == 0) {
            cfg->invert = 1;
        } else if (strcmp(arg, "--contrast") == 0) {
            NEED_VALUE("--contrast");
            if (parse_float("--contrast", value, &cfg->contrast)) return 1;
        } else if (strcmp(arg, "--brightness") == 0) {
            NEED_VALUE("--brightness");
            if (parse_float("--brightness", value, &cfg->brightness)) return 1;
        } else if (strcmp(arg, "--levels") == 0) {
            NEED_VALUE("--levels");
            if (parse_levels("--levels", value, &cfg->levels_black, &cfg->levels_white)) return 1;
        } else if (strcmp(arg, "--gamma") == 0) {
            NEED_VALUE("--gamma");
            if (parse_float("--gamma", value, &cfg->gamma)) return 1;
        } else if (strcmp(arg, "--saturation") == 0) {
            NEED_VALUE("--saturation");
            if (parse_float("--saturation", value, &cfg->saturation)) return 1;
        } else if (strcmp(arg, "--threshold") == 0) {
            NEED_VALUE("--threshold");
            if (parse_int("--threshold", value, &cfg->black_threshold)) return 1;
        } else if (strcmp(arg, "--gray-levels") == 0 || strcmp(arg, "--grey-levels") == 0) {
            NEED_VALUE("--gray-levels");
            if (parse_int("--gray-levels", value, &cfg->gray_levels)) return 1;
        } else if (strcmp(arg, "--color-step") == 0 || strcmp(arg, "--colour-step") == 0) {
            NEED_VALUE("--color-step");
            if (parse_int("--color-step", value, &cfg->color_step)) return 1;
        } else if (strcmp(arg, "--fg") == 0) {
            NEED_VALUE("--fg");
            if (parse_color("--fg", value, &cfg->fg)) return 1;
            cfg->fg_set = 1;
        } else if (strcmp(arg, "--output") == 0 || strcmp(arg, "-o") == 0) {
            NEED_VALUE("--output");
            cfg->output_path = value;
        } else if (arg[0] == '-' && arg[1] != '\0') {
            fprintf(stderr, "error: unknown option '%s' (try --help)\n", arg);
            return 1;
        } else {
            if (cfg->input_path != NULL) {
                fprintf(stderr, "error: more than one input file given ('%s' and '%s')\n",
                        cfg->input_path, arg);
                return 1;
            }
            cfg->input_path = arg;
        }
    }

    if (cfg->input_path == NULL) {
        fprintf(stderr, "error: no input image given (try --help)\n");
        return 1;
    }
    return 0;
}

#undef NEED_VALUE

int config_validate(const AsciiConfig *cfg) {
    size_t ramp_len = (cfg->charset != NULL) ? strlen(cfg->charset) : 0;

    if (cfg->output_width < 1 || cfg->output_width > 10000) {
        fprintf(stderr, "error: --width must be between 1 and 10000 (got %d)\n", cfg->output_width);
        return 1;
    }
    if (cfg->height_explicit && (cfg->output_height < 1 || cfg->output_height > 10000)) {
        fprintf(stderr, "error: --height must be between 1 and 10000 (got %d)\n", cfg->output_height);
        return 1;
    }
    if (!(cfg->char_aspect > 0.01f) || cfg->char_aspect > 10.0f) {
        fprintf(stderr, "error: --char-aspect must be between 0.01 and 10 (got %g)\n",
                (double)cfg->char_aspect);
        return 1;
    }
    if (ramp_len < 2) {
        fprintf(stderr, "error: character ramp must contain at least 2 characters\n");
        return 1;
    }
    if (!(cfg->contrast >= 0.0f) || cfg->contrast > 100.0f) {
        fprintf(stderr, "error: --contrast must be between 0 and 100 (got %g)\n",
                (double)cfg->contrast);
        return 1;
    }
    if (cfg->brightness < -255.0f || cfg->brightness > 255.0f) {
        fprintf(stderr, "error: --brightness must be between -255 and 255 (got %g)\n",
                (double)cfg->brightness);
        return 1;
    }
    if (!(cfg->levels_black >= 0.0f) || cfg->levels_black > 255.0f ||
        !(cfg->levels_white > cfg->levels_black) || cfg->levels_white > 255.0f) {
        fprintf(stderr, "error: --levels needs 0 <= LO < HI <= 255 (got %g,%g)\n",
                (double)cfg->levels_black, (double)cfg->levels_white);
        return 1;
    }
    if (!(cfg->gamma > 0.0f) || cfg->gamma > 10.0f) {
        fprintf(stderr, "error: --gamma must be between 0 and 10 (got %g)\n",
                (double)cfg->gamma);
        return 1;
    }
    if (!(cfg->saturation >= 0.0f) || cfg->saturation > 10.0f) {
        fprintf(stderr, "error: --saturation must be between 0 and 10 (got %g)\n",
                (double)cfg->saturation);
        return 1;
    }
    if (cfg->black_threshold < 0 || cfg->black_threshold > 255) {
        fprintf(stderr, "error: --threshold must be between 0 and 255 (got %d)\n",
                cfg->black_threshold);
        return 1;
    }
    if (cfg->gray_levels < 2 || cfg->gray_levels > 256) {
        fprintf(stderr, "error: --gray-levels must be between 2 and 256 (got %d)\n",
                cfg->gray_levels);
        return 1;
    }
    if (cfg->color_step < 1 || cfg->color_step > 128) {
        fprintf(stderr, "error: --color-step must be between 1 and 128 (got %d)\n",
                cfg->color_step);
        return 1;
    }
    return 0;
}

void config_print_help(FILE *out, const char *prog) {
    fprintf(out,
"ascii-art %s -- convert raster images to ASCII art\n"
"\n"
"Usage: %s <image> [options]\n"
"\n"
"Rendering mode\n"
"  --mode <mono|gray|color>  what carries the colour information (default: mono)\n"
"                            mono  glyph density only, one fixed foreground\n"
"                            gray  glyph density + per-glyph grey foreground\n"
"                            color glyph density + per-glyph RGB foreground\n"
"\n"
"Output\n"
"  --format <txt|ansi|html>  output encoding (default: txt)\n"
"  -o, --output <path>       write to a file (default: stdout)\n"
"      --bare                html: emit only the glyph markup, no <style> or\n"
"                            <pre> wrapper, for when the page already styles\n"
"                            .ascii-art and the g0..gN classes itself\n"
"\n"
"Spatial density (how many cells)\n"
"  --width <n>               output columns (default: 100)\n"
"  --height <n>              output rows (default: derived from aspect ratio)\n"
"  --char-aspect <f>         glyph width/height ratio\n"
"                            (default: 0.5 for txt/ansi, 0.83 for html)\n"
"\n"
"Tonal density (how many grey steps the glyphs can express)\n"
"  --charset <name>          built-in ramp: %s (default: medium)\n"
"  --charset-custom <chars>  custom ramp, ordered most ink first\n"
"\n"
"Tone (applied in this order: levels, gamma, contrast, brightness)\n"
"  --levels <LO,HI>          stretch input range LO..HI onto 0..255 (default: 0,255)\n"
"                            the only control that expands a narrow range --\n"
"                            use it when the subject is dark or low-contrast\n"
"  --gamma <f>               midtones: >1 lifts shadows, <1 deepens (default: 1.0)\n"
"  --brightness <f>          offset added after contrast, -255..255 (default: 0)\n"
"  --contrast <f>            scale around the midpoint, 0..100 (default: 1.0)\n"
"  --threshold <n>           luminance below this becomes a space, 0..255 (default: 0)\n"
"  --invert                  invert luminance (dark <-> light) before glyph choice\n"
"\n"
"Colour\n"
"  --saturation <f>          colour mode only: 0 = grey, 1 = source (default: 1.0)\n"
"  --gray-levels <n>         gray mode quantization steps, 2..256 (default: 16)\n"
"  --color-step <n>          colour mode quantization step, 1..128 (default: 16)\n"
"  --fg <#rrggbb>            mono mode foreground (default: inherit)\n"
"\n"
"Other\n"
"  -h, --help                show this help\n"
"      --version             show the version\n"
"\n"
"Examples\n"
"  %s photo.png --mode color --format ansi --width 160\n"
"  %s photo.png --mode gray --format html --width 260 --charset full \\\n"
"      --contrast 1.2 --brightness -5 --threshold 8 -o photo.html\n"
"  %s photo.png --mode mono --format txt --width 180 --charset simple -o photo.txt\n",
        ASCII_ART_VERSION, prog, charset_names(), prog, prog, prog);
}
