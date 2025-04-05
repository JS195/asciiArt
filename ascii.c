#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#include <stdio.h>

const char charArray[] = "$@B8&WM#*oahkbdpqwmZO0QLCJUYXzcvunxrjft/\\|()1{}[]?-_+~<>i!lI;:,\"^`'. ";
char number_to_custom_ascii(int num) {
    int index = (num * 72) / (255 + 1);
    return charArray[index];
}
int average(int R, int G, int B) {
    return (0.299 * R) + (0.587 * G) + (0.114 * B);
}

int main() {
    int width, height, channels;
    unsigned char *img = stbi_load("images\\owl.png", &width, &height, &channels, 0);
    if (img == NULL) {
        printf("Failed to load image\n");
        return 1;
    }
    int r = img[0];
    int g = img[1];
    int b = img[2];
    unsigned char *grayscaleImg = (unsigned char *)malloc(width * height);
    if (grayscaleImg == NULL) {
        printf("Memory allocation failed\n");
        stbi_image_free(img);
        return 1;
    }

    FILE *file = fopen("output.txt", "w");
    if (file == NULL) {
        printf("Error opening file for writing.\n");
        return 1;
    }
    // iterate pixels, conv to grey and assign ascii
    for (int j = 0; j < height; j+=2) {
        for (int i = 0; i < width; i+=2) {
            int index = (j * width + i) * channels;
            int gray = average(img[index], img[index + 1], img[index + 2]);
            grayscaleImg[j * width + i] = gray;
            
            char ch = number_to_custom_ascii(gray);
            fprintf(file, "%c", ch);
        }
        fprintf(file, "\n");
    }
    fclose(file);

    if (stbi_write_png("grayscale.png", width, height, 1, grayscaleImg, width) == 0) {
        printf("Failed to save image\n");
    } else {
        printf("Grayscale image saved as grayscale.png\n");
    }

    // Cleanup
    free(grayscaleImg);
    stbi_image_free(img);
    return 0;
}
