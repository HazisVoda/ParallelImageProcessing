#include "filters.h"
#include <stdint.h>

void box_blur_rows(const Image *src, Image *dst, int start_row, int end_row) {
    int w = src->width, h = src->height;
    for (int y = start_row; y < end_row; y++) {
        for (int x = 0; x < w; x++) {
            if (y == 0 || y == h-1 || x == 0 || x == w-1) {
                uint8_t *s = PIXEL(src, x, y);
                uint8_t *d = PIXEL(dst, x, y);
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                continue;
            }
            int r = 0, g = 0, b = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    uint8_t *p = PIXEL(src, x+dx, y+dy);
                    r += p[0]; g += p[1]; b += p[2];
                }
            }
            uint8_t *d = PIXEL(dst, x, y);
            d[0] = (uint8_t)(r / 9);
            d[1] = (uint8_t)(g / 9);
            d[2] = (uint8_t)(b / 9);
        }
    }
}

void box_blur(const Image *src, Image *dst) {
    box_blur_rows(src, dst, 0, src->height);
}

void sharpen_rows(const Image *src, Image *dst, int start_row, int end_row) {
    int w = src->width, h = src->height;
    for (int y = start_row; y < end_row; y++) {
        for (int x = 0; x < w; x++) {
            if (y == 0 || y == h-1 || x == 0 || x == w-1) {
                uint8_t *s = PIXEL(src, x, y);
                uint8_t *d = PIXEL(dst, x, y);
                d[0] = s[0]; d[1] = s[1]; d[2] = s[2];
                continue;
            }
            /* Laplacian sharpening kernel:
             *  0 -1  0
             * -1  5 -1
             *  0 -1  0
             */
            uint8_t *c  = PIXEL(src, x,   y  );
            uint8_t *u  = PIXEL(src, x,   y-1);
            uint8_t *dn = PIXEL(src, x,   y+1);
            uint8_t *l  = PIXEL(src, x-1, y  );
            uint8_t *r  = PIXEL(src, x+1, y  );
            for (int ch = 0; ch < 3; ch++) {
                int v = 5*c[ch] - u[ch] - dn[ch] - l[ch] - r[ch];
                if (v <   0) v = 0;
                if (v > 255) v = 255;
                PIXEL(dst, x, y)[ch] = (uint8_t)v;
            }
        }
    }
}

void sharpen(const Image *src, Image *dst) {
    sharpen_rows(src, dst, 0, src->height);
}
