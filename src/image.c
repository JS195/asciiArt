#include "image.h"

#include <stdio.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int image_load(Image *img, const char *path) {
    memset(img, 0, sizeof(*img));

    img->pixels = stbi_load(path, &img->width, &img->height, &img->channels, 0);
    if (img->pixels == NULL) {
        fprintf(stderr, "error: cannot read image '%s': %s\n", path, stbi_failure_reason());
        memset(img, 0, sizeof(*img));
        return 1;
    }
    if (img->width < 1 || img->height < 1 || img->channels < 1) {
        fprintf(stderr, "error: image '%s' has invalid dimensions %dx%d (%d channels)\n",
                path, img->width, img->height, img->channels);
        image_free(img);
        return 1;
    }
    return 0;
}

void image_free(Image *img) {
    if (img->pixels != NULL) stbi_image_free(img->pixels);
    memset(img, 0, sizeof(*img));
}
