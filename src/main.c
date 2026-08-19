/* ascii-art -- convert raster images to ASCII art for terminals and the web.
 *
 * IMAGE LOAD -> OUTPUT DIMENSIONS -> BLOCK RESAMPLE -> AVERAGE RGB ->
 * LUMINANCE -> BRIGHTNESS/CONTRAST -> THRESHOLD -> CHARACTER SELECTION ->
 * COLOUR -> QUANTIZATION -> TXT / ANSI / HTML
 */
#include <stdio.h>

#include "charset.h"
#include "config.h"
#include "image.h"
#include "output.h"
#include "render.h"

int main(int argc, char **argv) {
    AsciiConfig cfg;
    Charset cs;
    Image img;
    AsciiCanvas canvas;
    FILE *out = NULL;
    int rc;
    int status = 1;

    config_defaults(&cfg);

    rc = config_parse_args(&cfg, argc, argv);
    if (rc < 0) return 0; /* --help / --version */
    if (rc > 0) return 2;
    /* A terminal cell is ~2x taller than wide; the CSS this tool emits makes
     * the HTML cell nearly square. Using 0.5 for HTML squashes the image. */
    if (!cfg.char_aspect_set && cfg.format == FORMAT_HTML) {
        cfg.char_aspect = HTML_CHAR_ASPECT;
    }

    if (config_validate(&cfg) != 0) return 2;

    if (charset_init(&cs, cfg.charset_name, cfg.charset, cfg.charset_density) != 0) {
        fprintf(stderr, "error: character ramp must contain at least 2 characters\n");
        return 2;
    }

    if (image_load(&img, cfg.input_path) != 0) return 1;

    if (render_image(&img, &cfg, &cs, &canvas) != 0) goto cleanup_image;

    if (cfg.output_path != NULL) {
        out = fopen(cfg.output_path, "w");
        if (out == NULL) {
            fprintf(stderr, "error: cannot open '%s' for writing\n", cfg.output_path);
            goto cleanup_canvas;
        }
    } else {
        out = stdout;
    }

    if (output_write(out, &canvas, &cfg) != 0) goto cleanup_file;

    status = 0;

cleanup_file:
    if (out != NULL && out != stdout) {
        if (fclose(out) != 0 && status == 0) {
            fprintf(stderr, "error: failed to close '%s'\n", cfg.output_path);
            status = 1;
        }
    }
cleanup_canvas:
    canvas_free(&canvas);
cleanup_image:
    image_free(&img);
    return status;
}
