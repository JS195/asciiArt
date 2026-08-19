/* Self-contained assert tests for the pure parts of the pipeline.
 * Build and run with `make test`. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "charset.h"
#include "color.h"
#include "config.h"
#include "output.h"
#include "render.h"
#include "sample.h"

static int near(float a, float b) {
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d < 0.51f;
}

static void test_luminance(void) {
    RGB black = {0, 0, 0}, white = {255, 255, 255};
    RGB red = {255, 0, 0}, green = {0, 255, 0}, blue = {0, 0, 255};

    assert(near(luminance(black), 0.0f));
    assert(near(luminance(white), 255.0f));
    assert(near(luminance(red), 0.299f * 255.0f));
    assert(near(luminance(green), 0.587f * 255.0f));
    assert(near(luminance(blue), 0.114f * 255.0f));
    /* green must read brighter than red, which must read brighter than blue */
    assert(luminance(green) > luminance(red));
    assert(luminance(red) > luminance(blue));
}

static void test_character_selection(void) {
    Charset cs;
    size_t i;

    assert(charset_lookup(&cs, "full") == 0);
    assert(cs.length == strlen(cs.glyphs));
    assert(cs.length > 2);

    /* Ramp length is never hard-coded: max ink -> index 0, no ink -> last. */
    assert(charset_glyph(&cs, 1.0f) == cs.glyphs[0]);
    assert(charset_glyph(&cs, 0.0f) == cs.glyphs[cs.length - 1]);
    /* Out-of-range input is clamped, not read out of bounds. */
    assert(charset_glyph(&cs, 2.0f) == cs.glyphs[0]);
    assert(charset_glyph(&cs, -1.0f) == cs.glyphs[cs.length - 1]);

    /* Every index in the ramp must be reachable and monotonic in ink. */
    for (i = 0; i < cs.length; i++) {
        float ink = 1.0f - (float)i / (float)(cs.length - 1);
        assert(charset_glyph(&cs, ink) == cs.glyphs[i]);
    }

    assert(charset_lookup(&cs, "simple") == 0 && cs.length == 10);
    assert(charset_lookup(&cs, "medium") == 0);
    assert(charset_lookup(&cs, "nope") != 0);

    /* The "safe" ramp must never contain { or }: a fragment holding either is a
     * compile error when pasted into a Svelte/JSX component. */
    assert(charset_lookup(&cs, "safe") == 0);
    assert(strchr(cs.glyphs, '{') == NULL);
    assert(strchr(cs.glyphs, '}') == NULL);
    assert(cs.length == strlen(RAMP_FULL) - 2); /* full, minus the two braces */

    /* Custom ramps work and are still length-derived. */
    assert(charset_init(&cs, "custom", ".:-=+*#%@", NULL) == 0);
    assert(cs.length == 9);
    assert(charset_glyph(&cs, 1.0f) == '.');
    assert(charset_init(&cs, "custom", "x", NULL) != 0); /* too short */
}

static void test_measured_density_ramp(void) {
    /* The calibration hook: a density table overrides even index spacing. */
    static const float density[] = {0.9f, 0.2f, 0.0f};
    Charset cs;

    assert(charset_init(&cs, "measured", "#-.", density) == 0);
    assert(charset_glyph(&cs, 1.0f) == '#');
    assert(charset_glyph(&cs, 0.25f) == '-'); /* nearest measured coverage */
    assert(charset_glyph(&cs, 0.0f) == '.');
    /* Without the table the same ink would land on the middle glyph. */
    assert(charset_init(&cs, "uniform", "#-.", NULL) == 0);
    assert(charset_glyph(&cs, 0.25f) == '.');
}

static void test_brightness_contrast_clamping(void) {
    /* Contrast pivots on the midpoint. */
    assert(near(adjust_contrast(127.5f, 2.0f), 127.5f));
    assert(near(adjust_contrast(127.5f, 0.0f), 127.5f));
    assert(near(adjust_contrast(255.0f, 1.0f), 255.0f));
    assert(near(adjust_contrast(0.0f, 1.0f), 0.0f));

    /* Extremes clamp instead of wrapping or overflowing. */
    assert(near(adjust_contrast(255.0f, 4.0f), 255.0f));
    assert(near(adjust_contrast(0.0f, 4.0f), 0.0f));
    assert(near(adjust_contrast(200.0f, 10.0f), 255.0f));
    assert(near(adjust_contrast(50.0f, 10.0f), 0.0f));

    /* Contrast 0 flattens everything to mid grey. */
    assert(near(adjust_contrast(0.0f, 0.0f), 127.5f));
    assert(near(adjust_contrast(255.0f, 0.0f), 127.5f));

    /* Brightness is applied after contrast and clamps at both ends. */
    {
        Tone t = {0.0f, 255.0f, 1.0f, 1.0f, 20.0f};
        assert(near(apply_tone(100.0f, &t), 120.0f));
        t.brightness = -200.0f; assert(near(apply_tone(100.0f, &t), 0.0f));
        t.brightness = 255.0f;  assert(near(apply_tone(100.0f, &t), 255.0f));
        t.brightness = 0.0f;    assert(near(apply_tone(128.0f, &t), 128.0f));
        /* Default Tone is a complete no-op. */
        assert(near(apply_tone(37.0f, &t), 37.0f));
    }
}

static void test_levels(void) {
    /* Levels stretch a narrow input range onto the full output range -- the
     * thing contrast provably cannot do (see test below). */
    assert(near(apply_levels(18.0f, 18.0f, 130.0f), 0.0f));
    assert(near(apply_levels(130.0f, 18.0f, 130.0f), 255.0f));
    assert(near(apply_levels(74.0f, 18.0f, 130.0f), 127.5f));
    /* Outside the window clamps rather than wrapping. */
    assert(near(apply_levels(0.0f, 18.0f, 130.0f), 0.0f));
    assert(near(apply_levels(255.0f, 18.0f, 130.0f), 255.0f));
    /* Identity window is a no-op. */
    assert(near(apply_levels(99.0f, 0.0f, 255.0f), 99.0f));
    /* Degenerate window must not divide by zero. */
    assert(near(apply_levels(99.0f, 50.0f, 50.0f), 99.0f));

    /* The regression this feature exists for: a dark subject in [25,120].
     * Contrast alone collapses it, because adjust_contrast clamps to [0,1]
     * before brightness is ever added. */
    {
        Tone c = {0.0f, 255.0f, 1.0f, 3.2f, 231.0f}; /* the "equivalent" contrast trick */
        Tone l = {18.0f, 130.0f, 1.0f, 1.0f, 0.0f};  /* levels doing it properly */
        /* Two distinct shadow values, both below the contrast crush point. */
        assert(near(apply_tone(30.0f, &c), apply_tone(60.0f, &c))); /* detail gone */
        assert(apply_tone(60.0f, &l) - apply_tone(30.0f, &l) > 50.0f); /* detail kept */
    }
}

static void test_gamma(void) {
    assert(near(apply_gamma(128.0f, 1.0f), 128.0f)); /* no-op */
    assert(near(apply_gamma(0.0f, 2.2f), 0.0f));     /* endpoints are fixed */
    assert(near(apply_gamma(255.0f, 2.2f), 255.0f));
    assert(apply_gamma(64.0f, 2.2f) > 64.0f);        /* >1 lifts shadows */
    assert(apply_gamma(64.0f, 0.5f) < 64.0f);        /* <1 deepens them */
}

static void test_rgb_quantization(void) {
    RGB c = {200, 100, 50};
    RGB q = quantize_rgb(c, 32);
    assert(q.r == 192 && q.g == 96 && q.b == 32);

    /* step 1 is a no-op. */
    q = quantize_rgb(c, 1);
    assert(q.r == 200 && q.g == 100 && q.b == 50);

    /* Quantization is idempotent, which is what makes span runs stable. */
    q = quantize_rgb(quantize_rgb(c, 32), 32);
    assert(q.r == 192 && q.g == 96 && q.b == 32);

    /* Neighbouring colours collapse onto the same bucket -> longer runs. */
    {
        RGB a = {201, 101, 51}, b = {223, 127, 63};
        RGB qa = quantize_rgb(a, 32), qb = quantize_rgb(b, 32);
        assert(qa.r == qb.r && qa.g == qb.g && qa.b == qb.b);
    }
}

static void test_gray_quantization(void) {
    assert(quantize_gray(0.0f, 16) == 0);
    assert(quantize_gray(255.0f, 16) == 15);
    assert(quantize_gray(128.0f, 16) == 8);
    assert(quantize_gray(-10.0f, 16) == 0);   /* clamped */
    assert(quantize_gray(999.0f, 16) == 15);  /* clamped */
    assert(quantize_gray(128.0f, 2) == 1);

    /* Level values span the full 0..255 range so CSS classes hit both ends. */
    assert(gray_level_value(0, 16) == 0);
    assert(gray_level_value(15, 16) == 255);
    assert(gray_level_value(99, 16) == 255); /* clamped */
    assert(gray_level_value(0, 2) == 0);
    assert(gray_level_value(1, 2) == 255);
}

static void test_saturation(void) {
    RGB c = {200, 100, 50};
    RGB gray = adjust_saturation(c, 0.0f);
    RGB same = adjust_saturation(c, 1.0f);

    assert(gray.r == gray.g && gray.g == gray.b);
    assert(near((float)gray.r, luminance(c)));
    assert(same.r == c.r && same.g == c.g && same.b == c.b);

    /* Boosting must clamp, not wrap. */
    {
        RGB hot = adjust_saturation(c, 10.0f);
        assert(hot.r == 255 && hot.b == 0);
    }
}

static void test_output_dimensions(void) {
    AsciiConfig cfg;
    int w, h;

    config_defaults(&cfg);
    cfg.output_width = 50;
    cfg.char_aspect = 0.5f;

    /* 100x200 source: (200/100) * 50 * 0.5 = 50 rows. */
    assert(compute_output_dimensions(&cfg, 100, 200, &w, &h) == 0);
    assert(w == 50 && h == 50);

    /* Square source halves the rows because glyphs are tall. */
    assert(compute_output_dimensions(&cfg, 100, 100, &w, &h) == 0);
    assert(w == 50 && h == 25);

    /* char_aspect 1.0 means square cells -> no correction. */
    cfg.char_aspect = 1.0f;
    assert(compute_output_dimensions(&cfg, 100, 100, &w, &h) == 0);
    assert(w == 50 && h == 50);

    /* Explicit height wins. */
    cfg.height_explicit = 1;
    cfg.output_height = 120;
    assert(compute_output_dimensions(&cfg, 100, 200, &w, &h) == 0);
    assert(w == 50 && h == 120);

    /* A very wide, very short source still yields at least one row. */
    cfg.height_explicit = 0;
    cfg.char_aspect = 0.5f;
    cfg.output_width = 4;
    assert(compute_output_dimensions(&cfg, 10000, 1, &w, &h) == 0);
    assert(h == 1);

    /* Absurd sizes are refused rather than attempting a huge allocation. */
    cfg.output_width = 10000;
    cfg.height_explicit = 1;
    cfg.output_height = 10000;
    assert(compute_output_dimensions(&cfg, 100, 100, &w, &h) != 0);
}

/* The rendered picture must have the same shape as the source. This is the
 * regression that made HTML output look squashed: the emitted CSS uses a tight
 * line-height, so an HTML cell is nearly square, and the terminal's 0.5 made
 * every render ~40% too short. */
static void test_html_proportions(void) {
    AsciiConfig cfg;
    int w, h, i;
    const int sizes[][2] = {{1264, 842}, {1000, 1000}, {600, 900}, {1920, 1080}};

    for (i = 0; i < 4; i++) {
        double src_w = sizes[i][0], src_h = sizes[i][1], drawn_w, drawn_h, err;

        config_defaults(&cfg);
        cfg.char_aspect = HTML_CHAR_ASPECT; /* what main() picks for FORMAT_HTML */
        cfg.output_width = 300;
        assert(compute_output_dimensions(&cfg, sizes[i][0], sizes[i][1], &w, &h) == 0);

        /* Physical size of the rendered block, in px. */
        drawn_w = w * HTML_FONT_SIZE_PX * HTML_GLYPH_ADVANCE;
        drawn_h = h * HTML_FONT_SIZE_PX * HTML_LINE_HEIGHT;

        err = (drawn_h / drawn_w) / (src_h / src_w);
        assert(err > 0.98 && err < 1.02); /* within 2% of the source shape */
    }

    /* And the terminal default must NOT be used for HTML: 0.5 is what caused
     * the squash, so it has to be visibly wrong for these proportions. */
    config_defaults(&cfg);
    cfg.output_width = 300;
    assert(compute_output_dimensions(&cfg, 1264, 842, &w, &h) == 0);
    {
        double drawn_w = w * HTML_FONT_SIZE_PX * HTML_GLYPH_ADVANCE;
        double drawn_h = h * HTML_FONT_SIZE_PX * HTML_LINE_HEIGHT;
        double err = (drawn_h / drawn_w) / (842.0 / 1264.0);
        assert(err < 0.7); /* ~40% too short, as observed in the browser */
    }
}

static void test_sample_block(void) {
    /* 2x2 RGB: red, green / blue, white. */
    unsigned char rgb[2 * 2 * 3] = {
        255, 0, 0,   0, 255, 0,
        0, 0, 255,   255, 255, 255
    };
    RGB avg = sample_block(rgb, 2, 2, 3, 0, 0, 2, 2);
    assert(avg.r == 127 && avg.g == 127 && avg.b == 127);

    /* A single-pixel rect returns that pixel. */
    avg = sample_block(rgb, 2, 2, 3, 1, 0, 2, 1);
    assert(avg.r == 0 && avg.g == 255 && avg.b == 0);

    /* Degenerate rects are widened to 1x1 instead of dividing by zero. */
    avg = sample_block(rgb, 2, 2, 3, 1, 1, 1, 1);
    assert(avg.r == 255 && avg.g == 255 && avg.b == 255);

    /* Out-of-range rects clamp to the image. */
    avg = sample_block(rgb, 2, 2, 3, -5, -5, 99, 99);
    assert(avg.r == 127 && avg.g == 127 && avg.b == 127);

    /* Grey and grey+alpha replicate the luma channel; alpha is ignored. */
    {
        unsigned char g1[2] = {10, 200};
        unsigned char g2[4] = {10, 255, 200, 0};
        avg = sample_block(g1, 2, 1, 1, 0, 0, 2, 1);
        assert(avg.r == 105 && avg.g == 105 && avg.b == 105);
        avg = sample_block(g2, 2, 1, 2, 0, 0, 2, 1);
        assert(avg.r == 105 && avg.g == 105 && avg.b == 105);
    }

    /* RGBA ignores alpha rather than reading past the pixel. */
    {
        unsigned char rgba[2 * 4] = {255, 0, 0, 0,  0, 0, 255, 255};
        avg = sample_block(rgba, 2, 1, 4, 0, 0, 2, 1);
        assert(avg.r == 127 && avg.g == 0 && avg.b == 127);
    }
}

/* Builds a 4x1 greyscale image: black, dark, light, white. */
static void make_strip(Image *img, unsigned char *buf) {
    buf[0] = 0; buf[1] = 60; buf[2] = 200; buf[3] = 255;
    img->pixels = buf;
    img->width = 4;
    img->height = 1;
    img->channels = 1;
}

static void test_render_and_inversion(void) {
    unsigned char buf[4];
    Image img;
    AsciiConfig cfg;
    Charset cs;
    AsciiCanvas canvas;

    make_strip(&img, buf);
    config_defaults(&cfg);
    cfg.output_width = 4;
    cfg.height_explicit = 1;
    cfg.output_height = 1;
    cfg.mode = MODE_GRAYSCALE;
    assert(charset_lookup(&cs, "full") == 0);

    /* Default: bright source -> heaviest glyph, black source -> lightest. */
    assert(render_image(&img, &cfg, &cs, &canvas) == 0);
    assert(canvas.width == 4 && canvas.height == 1);
    assert(canvas.cells[3].glyph == cs.glyphs[0]);              /* white */
    assert(canvas.cells[0].glyph == cs.glyphs[cs.length - 1]);  /* black */
    /* Grey mode also carries brightness in the foreground. */
    assert(canvas.cells[3].color.r > canvas.cells[0].color.r);
    assert(canvas.cells[3].gray_level == cfg.gray_levels - 1);
    assert(canvas.cells[0].gray_level == 0);
    canvas_free(&canvas);
    assert(canvas.cells == NULL);

    /* --invert swaps which end of the ramp counts as ink... */
    cfg.invert = 1;
    assert(render_image(&img, &cfg, &cs, &canvas) == 0);
    assert(canvas.cells[0].glyph == cs.glyphs[0]);              /* black -> heavy */
    assert(canvas.cells[3].glyph == cs.glyphs[cs.length - 1]);  /* white -> light */
    /* ...but the foreground still tracks source brightness, so an inverted
     * render stays legible on a light page instead of going white-on-white. */
    assert(canvas.cells[0].color.r == 0);
    assert(canvas.cells[3].color.r == 255);
    assert(canvas.cells[3].color.r > canvas.cells[0].color.r);
    canvas_free(&canvas);

    /* Threshold follows the ink direction: under --invert it blanks the bright
     * background rather than the dark subject. */
    cfg.black_threshold = 80;
    assert(render_image(&img, &cfg, &cs, &canvas) == 0);
    assert(canvas.cells[0].glyph != ' '); /* black subject survives */
    assert(canvas.cells[3].glyph == ' '); /* white background blanked */
    canvas_free(&canvas);
    cfg.black_threshold = 0;

    /* Threshold blanks near-black cells (60 < 80) but keeps brighter ones. */
    cfg.invert = 0;
    cfg.black_threshold = 80;
    assert(render_image(&img, &cfg, &cs, &canvas) == 0);
    assert(canvas.cells[0].glyph == ' ');
    assert(canvas.cells[1].glyph == ' ');
    assert(canvas.cells[2].glyph != ' ');
    canvas_free(&canvas);

    /* Colour mode keeps the glyph from luminance and colour from the source. */
    {
        unsigned char rgb[2 * 3] = {255, 0, 0,  255, 255, 255};
        Image cimg;
        cimg.pixels = rgb; cimg.width = 2; cimg.height = 1; cimg.channels = 3;
        cfg.mode = MODE_COLOR;
        cfg.black_threshold = 0;
        cfg.output_width = 2;
        cfg.saturation = 1.0f;
        cfg.color_step = 1;
        assert(render_image(&cimg, &cfg, &cs, &canvas) == 0);
        assert(canvas.cells[0].color.r == 255 && canvas.cells[0].color.g == 0);
        assert(canvas.cells[0].gray_level == -1);
        /* The white cell is brighter, so it gets the heavier glyph. */
        assert(canvas.cells[1].glyph == cs.glyphs[0]);
        canvas_free(&canvas);
    }
}

static void test_downscale_averages_not_samples(void) {
    /* A 4x1 checker of black/white must average to mid grey when squeezed into
     * one cell. Pixel-skipping would return pure black or pure white. */
    unsigned char buf[4] = {0, 255, 0, 255};
    Image img;
    AsciiConfig cfg;
    Charset cs;
    AsciiCanvas canvas;

    img.pixels = buf; img.width = 4; img.height = 1; img.channels = 1;
    config_defaults(&cfg);
    cfg.output_width = 1;
    cfg.height_explicit = 1;
    cfg.output_height = 1;
    cfg.mode = MODE_GRAYSCALE;
    assert(charset_lookup(&cs, "full") == 0);

    assert(render_image(&img, &cfg, &cs, &canvas) == 0);
    assert(canvas.cells[0].color.r > 100 && canvas.cells[0].color.r < 155);
    canvas_free(&canvas);
}

/* The emitted block's bounding box must equal the ink's, or a page that centres
 * it puts the picture off-centre by half the leading margin. Trailing blanks
 * were always trimmed; leading ones have to be trimmed as a block to match. */
static void test_output_is_tightly_cropped(void) {
    /* 8x5 grey image with ink in the middle columns of the middle row only:
     * 3 blank columns left, 2 right, 2 blank rows above and below. Untrimmed,
     * the box would be 8x5 while the ink is one 3-wide row. Both axes matter:
     * a page aligning to the block's edges must land on the picture. */
    unsigned char buf[8 * 5];
    Image img;
    AsciiConfig cfg;
    Charset cs;
    AsciiCanvas canvas;
    FILE *f;
    char line[256];
    int y, x, rows;

    for (y = 0; y < 5; y++)
        for (x = 0; x < 8; x++)
            buf[y * 8 + x] = (y == 2 && x >= 3 && x < 6) ? 255 : 0;

    img.pixels = buf; img.width = 8; img.height = 5; img.channels = 1;

    config_defaults(&cfg);
    cfg.output_width = 8;
    cfg.height_explicit = 1;
    cfg.output_height = 5;
    cfg.format = FORMAT_TXT;
    assert(charset_lookup(&cs, "full") == 0);
    assert(render_image(&img, &cfg, &cs, &canvas) == 0);

    f = tmpfile();
    assert(f != NULL);
    assert(output_write(f, &canvas, &cfg) == 0);
    rewind(f);

    assert(fgets(line, sizeof(line), f) != NULL);
    /* No leading blank column survives... */
    assert(line[0] != ' ');
    /* ...and the line is exactly as wide as the ink (3 columns). */
    assert(strcspn(line, "\n") == 3);

    /* No blank row above (the first line read is already the inked one) and
     * none below: the ink occupies exactly one row, so that is all we get. */
    rows = 1;
    while (fgets(line, sizeof(line), f) != NULL) rows++;
    assert(rows == 1);

    fclose(f);
    canvas_free(&canvas);
}

static void test_config_validation(void) {
    AsciiConfig cfg;

    config_defaults(&cfg);
    cfg.input_path = "x.png";
    assert(config_validate(&cfg) == 0);

    /* Every knob that could divide by zero or blow up an allocation is caught. */
    config_defaults(&cfg); cfg.output_width = 0;      assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.output_width = 99999;  assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.char_aspect = 0.0f;    assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.char_aspect = -1.0f;   assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.gray_levels = 1;       assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.color_step = 0;        assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.black_threshold = 256; assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.saturation = -1.0f;    assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.contrast = -1.0f;      assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.gamma = 0.0f;          assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.levels_black = 200.0f; cfg.levels_white = 100.0f;
    assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.levels_white = 300.0f; assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.charset = "x";         assert(config_validate(&cfg) != 0);
    config_defaults(&cfg); cfg.height_explicit = 1; cfg.output_height = 0;
    assert(config_validate(&cfg) != 0);
}

static void test_arg_parsing(void) {
    AsciiConfig cfg;
    char *ok[] = {"ascii-art", "my photo.png", "--mode", "color", "--format", "html",
                  "--width", "260", "--threshold", "8", "--saturation", "0.65"};
    char *bad_value[] = {"ascii-art", "a.png", "--width", "wide"};
    char *missing_value[] = {"ascii-art", "a.png", "--width"};
    char *unknown[] = {"ascii-art", "a.png", "--nope"};
    char *no_input[] = {"ascii-art", "--width", "10"};

    config_defaults(&cfg);
    assert(config_parse_args(&cfg, 12, ok) == 0);
    assert(strcmp(cfg.input_path, "my photo.png") == 0); /* spaces survive */
    assert(cfg.mode == MODE_COLOR && cfg.format == FORMAT_HTML);
    assert(cfg.output_width == 260 && cfg.black_threshold == 8);
    assert(cfg.saturation > 0.649f && cfg.saturation < 0.651f);

    config_defaults(&cfg); assert(config_parse_args(&cfg, 4, bad_value) > 0);
    config_defaults(&cfg); assert(config_parse_args(&cfg, 3, missing_value) > 0);
    config_defaults(&cfg); assert(config_parse_args(&cfg, 3, unknown) > 0);
    config_defaults(&cfg); assert(config_parse_args(&cfg, 3, no_input) > 0);

    /* --charset-custom overrides the built-in ramp. */
    {
        char *custom[] = {"ascii-art", "a.png", "--charset-custom", "@. "};
        config_defaults(&cfg);
        assert(config_parse_args(&cfg, 4, custom) == 0);
        assert(strcmp(cfg.charset, "@. ") == 0);
        assert(cfg.charset_density == NULL);
    }
}

int main(void) {
    test_luminance();
    test_character_selection();
    test_measured_density_ramp();
    test_brightness_contrast_clamping();
    test_levels();
    test_gamma();
    test_rgb_quantization();
    test_gray_quantization();
    test_saturation();
    test_output_dimensions();
    test_html_proportions();
    test_sample_block();
    test_render_and_inversion();
    test_downscale_averages_not_samples();
    test_output_is_tightly_cropped();
    test_config_validation();
    test_arg_parsing();

    printf("all tests passed\n");
    return 0;
}
