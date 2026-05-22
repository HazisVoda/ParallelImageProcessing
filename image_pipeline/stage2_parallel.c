#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "ppm.h"
#include "filters.h"
#include "timer.h"

typedef struct {
    const Image *src;
    Image       *dst;
    int          start_row;
    int          end_row;    /* exclusive */
} StripArgs;

static void *blur_strip(void *arg) {
    StripArgs *a = arg;
    box_blur_rows(a->src, a->dst, a->start_row, a->end_row);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s input.ppm output.ppm num_threads\n", argv[0]);
        return 1;
    }

    int nthreads = atoi(argv[3]);
    if (nthreads < 1) {
        fprintf(stderr, "num_threads must be >= 1\n");
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

    pthread_t *threads = malloc((size_t)nthreads * sizeof(pthread_t));
    StripArgs *args    = malloc((size_t)nthreads * sizeof(StripArgs));
    if (!threads || !args) {
        fprintf(stderr, "out of memory\n");
        image_free(src); image_free(dst);
        free(threads); free(args);
        return 1;
    }

    int rows_per_thread = src->height / nthreads;

    GET_TIME(t0);
    for (int i = 0; i < nthreads; i++) {
        args[i].src       = src;
        args[i].dst       = dst;
        args[i].start_row = i * rows_per_thread;
        /* last thread takes any leftover rows from integer division */
        args[i].end_row   = (i == nthreads - 1) ? src->height
                                                 : (i + 1) * rows_per_thread;
        pthread_create(&threads[i], NULL, blur_strip, &args[i]);
    }
    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);
    GET_TIME(t1);
    printf("blur:  %.4f s  (%d thread%s)\n",
           t1 - t0, nthreads, nthreads == 1 ? "" : "s");

    GET_TIME(t0);
    int err = ppm_save(dst, argv[2]);
    GET_TIME(t1);
    if (!err) printf("save:  %.4f s\n", t1 - t0);

    free(threads);
    free(args);
    image_free(src);
    image_free(dst);
    return err;
}
