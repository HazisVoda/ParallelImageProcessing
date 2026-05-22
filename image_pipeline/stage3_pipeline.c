#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif
#include "ppm.h"
#include "filters.h"
#include "queue.h"
#include "timer.h"

/* ---- per-stage argument structs ---- */

typedef struct {
    char      **paths;
    int         n;
    ImageQueue *out_q;
} LoaderArgs;

typedef struct {
    ImageQueue *in_q;
    ImageQueue *out_q;
} FilterArgs;

typedef struct {
    ImageQueue *in_q;
    const char *out_dir;
} SaverArgs;

/* ---- thread functions ---- */

static void *stage_load(void *arg) {
    LoaderArgs *a = arg;
    for (int i = 0; i < a->n; i++) {
        Image *img = ppm_load(a->paths[i]);
        if (img) queue_put(a->out_q, img);
    }
    queue_finish(a->out_q);
    return NULL;
}

static void *stage_blur(void *arg) {
    FilterArgs *a = arg;
    Image *src;
    while ((src = queue_take(a->in_q)) != NULL) {
        Image *dst = image_alloc(src->width, src->height);
        if (dst) box_blur(src, dst);
        image_free(src);
        if (dst) queue_put(a->out_q, dst);
    }
    queue_finish(a->out_q);
    return NULL;
}

static void *stage_sharpen(void *arg) {
    FilterArgs *a = arg;
    Image *src;
    while ((src = queue_take(a->in_q)) != NULL) {
        Image *dst = image_alloc(src->width, src->height);
        if (dst) sharpen(src, dst);
        image_free(src);
        if (dst) queue_put(a->out_q, dst);
    }
    queue_finish(a->out_q);
    return NULL;
}

static void *stage_save(void *arg) {
    SaverArgs *a = arg;
    Image *img;
    int n = 0;
    char path[512];
    while ((img = queue_take(a->in_q)) != NULL) {
        snprintf(path, sizeof(path), "%s/out_%04d.ppm", a->out_dir, n++);
        ppm_save(img, path);
        image_free(img);
    }
    return NULL;
}

/* ---- main ---- */

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s output_dir input1.ppm [input2.ppm ...]\n", argv[0]);
        return 1;
    }

    const char *out_dir     = argv[1];
    char      **input_paths = argv + 2;
    int         n_inputs    = argc - 2;

    MKDIR(out_dir);

    ImageQueue *q1 = queue_create(4);
    ImageQueue *q2 = queue_create(4);
    ImageQueue *q3 = queue_create(4);
    if (!q1 || !q2 || !q3) { fprintf(stderr, "out of memory\n"); return 1; }

    LoaderArgs la = { input_paths, n_inputs, q1 };
    FilterArgs ba = { q1, q2 };
    FilterArgs sa = { q2, q3 };
    SaverArgs  wa = { q3, out_dir };

    pthread_t threads[4];
    double t0, t1;

    GET_TIME(t0);
    pthread_create(&threads[0], NULL, stage_load,    &la);
    pthread_create(&threads[1], NULL, stage_blur,    &ba);
    pthread_create(&threads[2], NULL, stage_sharpen, &sa);
    pthread_create(&threads[3], NULL, stage_save,    &wa);
    for (int i = 0; i < 4; i++)
        pthread_join(threads[i], NULL);
    GET_TIME(t1);

    printf("pipeline: %d images in %.3f s  (%.1f img/s)\n",
           n_inputs, t1 - t0, n_inputs / (t1 - t0));

    queue_destroy(q1);
    queue_destroy(q2);
    queue_destroy(q3);
    return 0;
}
