#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ppm.h"

/* Generates a 1920x1080 test image with smooth colour gradients and circles.
 * Gives the blur filter something visually interesting to work on. */
int main(int argc, char *argv[]) {
    const char *path = argc > 1 ? argv[1] : "input.ppm";
    int w = 1920, h = 1080;

    Image *img = image_alloc(w, h);
    if (!img) { fprintf(stderr, "out of memory\n"); return 1; }

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            /* background: smooth gradient */
            int r = (int)(255.0 * x / w);
            int g = (int)(255.0 * y / h);
            int b = 128;

            /* draw a few circles */
            int cx = w/2, cy = h/2;
            double dist = sqrt((double)(x-cx)*(x-cx) + (double)(y-cy)*(y-cy));
            if ((int)dist % 80 < 8) { r = 255; g = 255; b = 255; }

            /* add a sharp diagonal stripe to make blur visible */
            if ((x + y) % 40 < 4) { r = 0; g = 0; b = 0; }

            uint8_t *p = PIXEL(img, x, y);
            p[0] = (uint8_t)r;
            p[1] = (uint8_t)g;
            p[2] = (uint8_t)b;
        }
    }

    int err = ppm_save(img, path);
    if (!err) printf("wrote %s (%dx%d)\n", path, w, h);
    image_free(img);
    return err;
}
