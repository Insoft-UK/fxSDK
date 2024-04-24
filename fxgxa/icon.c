#include <stdio.h>
#include <string.h>
#include <fxgxa.h>
#include <png.h>
#include <endianness.h>

uint8_t *icon_load(const char *filename, int *width, int *height)
{
    png_image img;
    memset(&img, 0, sizeof img);
    img.opaque = NULL;
    img.version = PNG_IMAGE_VERSION;

    void *buffer = NULL;

    png_image_begin_read_from_file(&img, filename);
    if(img.warning_or_error) {
        fprintf(stderr, "libpng %s: %s\n", img.warning_or_error == 1
            ? "warning": "error", img.message);
        if(img.warning_or_error > 1) goto err;
    }

    img.format = PNG_FORMAT_RGB;

    buffer = calloc(img.width * img.height, 3);
    if(!buffer) {
        fprintf(stderr, "error: cannot read %s: %m\n", filename);
        goto err;
    }

    png_image_finish_read(&img, NULL, buffer, 0, NULL);
    if(img.warning_or_error) {
        fprintf(stderr, "libpng %s: %s\n", img.warning_or_error == 1
            ? "warning": "error", img.message);
        if(img.warning_or_error > 1) goto err;
    }
    if(width) *width = img.width;
    if(height) *height = img.height;

    png_image_free(&img);
    return buffer;

err:
    png_image_free(&img);
    free(buffer);
    return NULL;
}

int icon_save(const char *filename, uint8_t *input, int width, int height)
{
    png_image img;
    memset(&img, 0, sizeof img);

    img.version = PNG_IMAGE_VERSION;
    img.width = width;
    img.height = height;
    img.format = PNG_FORMAT_RGB;

    png_image_write_to_file(&img, filename, 0, input, 0, NULL);
    png_image_free(&img);

    if(img.warning_or_error) {
        fprintf(stderr, "libpng %s: %s\n", img.warning_or_error == 1
            ? "warning": "error", img.message);
        if(img.warning_or_error > 1) return 1;
    }

    return 0;
}

uint8_t *icon_conv_24to1(uint8_t const *rgb24, int width, int height)
{
    int bytes_per_row = (width + 7) >> 3;
    uint8_t *mono = calloc(bytes_per_row * height, 1);
    if(!mono) return NULL;

    for(int y = 0; y < height; y++)
    for(int x = 0; x < width; x++) {
        int in = 3 * (y * width + x);
        int out = (y * bytes_per_row) + (x >> 3);
        int color = (rgb24[in] + rgb24[in+1] + rgb24[in+2]) < 384;
        mono[out] |= color << (~x & 7);
    }

    return mono;
}

uint8_t *icon_conv_1to24(uint8_t const *mono, int width, int height)
{
    int bytes_per_row = (width + 7) >> 3;
    uint8_t *rgb24 = calloc(width * height, 3);
    if(!rgb24) return NULL;

    for(int y = 0; y < height; y++)
    for(int x = 0; x < width; x++) {
        int in = (y * bytes_per_row) + (x >> 3);
        int out = 3 * (y * width + x);
        int color = (mono[in] & (0x80 >> (x & 7))) != 0 ? 0x00 : 0xff;
        rgb24[out]   = color;
        rgb24[out+1] = color;
        rgb24[out+2] = color;
    }

    return rgb24;
}

uint16_t *icon_conv_24to16(uint8_t const *rgb24, int width, int height)
{
    uint16_t *rgb16be = calloc(width * height, 2);
    if(!rgb16be) return NULL;

    for(int y = 0; y < height; y++)
    for(int x = 0; x < width; x++) {
        int in = 3 * (y * width + x);
        int color = ((rgb24[in]   & 0xf8) << 8)
                  | ((rgb24[in+1] & 0xfc) << 3)
                  | ((rgb24[in+2] & 0xf8) >> 3);
        rgb16be[y * width + x] = htobe16(color);
    }

    return rgb16be;
}

uint8_t *icon_conv_16to24(uint16_t const *rgb16be, int width, int height)
{
    uint8_t *rgb24 = calloc(width * height, 3);
    if(!rgb24) return NULL;

    for(int y = 0; y < height; y++)
    for(int x = 0; x < width; x++) {
        int out = 3 * (y * width + x);
        int color = be16toh(rgb16be[y * width + x]);
        rgb24[out]   = (color & 0xf800) >> 8;
        rgb24[out+1] = (color & 0x07e0) >> 3;
        rgb24[out+2] = (color & 0x001f) << 3;
    }

    return rgb24;
}

void icon_print_1(uint8_t const *mono, int width, int height)
{
    int bytes_per_row = (width + 7) >> 3;

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int in = (y * bytes_per_row) + (x >> 3);
            int color = (mono[in] & (0x80 >> (x & 7))) != 0;
            putchar(color ? '#' : ' ');
            putchar(color ? '#' : ' ');
        }
        putchar('\n');
    }
}

void icon_print_16(uint16_t const *rgb16be, int width, int height)
{
    uint8_t *rgb24 = icon_conv_16to24(rgb16be, width, height);
    if(!rgb24) {
        fprintf(stderr, "error: %m\n");
        return;
    };

    for(int y = 0; y < height; y++) {
        for(int x = 0; x < width; x++) {
            int in = 3 * (y * width + x);
            int r=rgb24[in], g=rgb24[in+1], b=rgb24[in+2];
            printf("\e[48;2;%d;%d;%dm  ", r, g, b);
        }
        printf("\e[0m\n");
    }

    free(rgb24);
}
