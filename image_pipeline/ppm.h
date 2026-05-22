#ifndef PPM_H
#define PPM_H

#include <stdint.h>   // for uint8_t

/*
 * An image in memory: width and height in pixels, plus a flat array
 * of RGB bytes laid out interleaved (R, G, B, R, G, B, ...) row by row.
 *
 * For a pixel at (x, y), its three bytes start at offset
 *     (y * width + x) * 3
 * in the data array. The PIXEL macro below hides this arithmetic.
 *
 * The struct is always heap-allocated and owned through a pointer.
 * image_alloc creates one, image_free destroys one.
 */
typedef struct {
    int      width;
    int      height;
    uint8_t *data;
} Image;

/*
 * Pointer to the first byte (the red channel) of the pixel at (x, y).
 * The next two bytes after it are green and blue.
 *
 * Usage:
 *     PIXEL(img, x, y)[0] = 255;   // set red
 *     PIXEL(img, x, y)[1] = 128;   // set green
 *     PIXEL(img, x, y)[2] = 0;     // set blue
 *
 * No bounds checking. Caller is responsible for keeping x in [0, width)
 * and y in [0, height).
 */
#define PIXEL(img, x, y) ((img)->data + ((y) * (img)->width + (x)) * 3)

/*
 * Allocate a new Image of the given dimensions. The data array is
 * malloc'd but its contents are uninitialised — fill it in yourself
 * or call a filter that writes to every pixel.
 *
 * Returns NULL on allocation failure.
 */
Image *image_alloc(int width, int height);

/*
 * Free both the data array and the Image struct itself. Safe to call
 * with NULL (does nothing in that case).
 */
void image_free(Image *img);

/*
 * Load a P6 binary PPM file from disk. Returns a newly allocated
 * Image on success, or NULL if the file can't be opened, isn't a
 * valid P6 PPM, or memory allocation fails.
 *
 * Caller owns the returned Image and must eventually call image_free.
 */
Image *ppm_load(const char *path);

/*
 * Save an Image as a P6 binary PPM file. Returns 0 on success,
 * nonzero on failure (can't open file, write error, etc.).
 */
int ppm_save(const Image *img, const char *path);

#endif  /* PPM_H */