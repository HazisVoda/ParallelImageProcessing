#include <stdio.h>
#include <stdlib.h>
#include "ppm.h"
#include "filters.h"
#include "timer.h"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s input.ppm output.ppm\n", argv[0]);
        return 1;
    }

    double t0, t1;

    GET_TIME(t0);
    Image *src = ppm_load(argv[1]);
    GET_TIME(t1);
    if (!src) return 1;
    printf("load:  %.4f s  (%d x %d)\n", t1 - t0, src->width, src->height);

    Image *dst = image_alloc(src->width, src->height);
    if (!dst) { image_free(src); return 1; }

    GET_TIME(t0);
    box_blur(src, dst);
    GET_TIME(t1);
    printf("blur:  %.4f s\n", t1 - t0);

    GET_TIME(t0);
    int err = ppm_save(dst, argv[2]);
    GET_TIME(t1);
    if (!err) printf("save:  %.4f s\n", t1 - t0);

    image_free(src);
    image_free(dst);
    return err;
}
