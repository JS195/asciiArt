/* image.h -- image loading (stb_image) and ownership. */
#ifndef ASCII_IMAGE_H
#define ASCII_IMAGE_H

typedef struct {
    unsigned char *pixels; /* width * height * channels, 8 bits per channel */
    int width;
    int height;
    int channels; /* 1 = grey, 2 = grey+alpha, 3 = RGB, 4 = RGBA */
} Image;

/* Loads any format stb_image supports. Returns 0 on success, non-zero on
 * failure (message written to stderr). On failure `img` is zeroed. */
int image_load(Image *img, const char *path);

void image_free(Image *img);

#endif /* ASCII_IMAGE_H */
