#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <direct.h>
#define MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define MKDIR(p) mkdir((p), 0755)
#endif
#include "ppm.h"
#include "filters.h"
#include "timer.h"

/* Serial baseline for Stage 3: same work as the pipeline (load+blur+sharpen+save)
 * but processed one image at a time. Compare total time against stage3_pipeline. */
int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s output_dir input1.ppm [input2.ppm ...]\n", argv[0]);
        return 1;
    }

    const char *out_dir     = argv[1];
    char      **input_paths = argv + 2;
    int         n_inputs    = argc - 2;

    MKDIR(out_dir);

    double t0, t1;
    GET_TIME(t0);

    for (int i = 0; i < n_inputs; i++) {
        Image *src = ppm_load(input_paths[i]);
        if (!src) continue;

        Image *blurred = image_alloc(src->width, src->height);
        box_blur(src, blurred);
        image_free(src);

        Image *sharpened = image_alloc(blurred->width, blurred->height);
        sharpen(blurred, sharpened);
        image_free(blurred);

        char path[512];
        snprintf(path, sizeof(path), "%s/out_%04d.ppm", out_dir, i);
        ppm_save(sharpened, path);
        image_free(sharpened);
    }

    GET_TIME(t1);
    printf("serial:   %d images in %.3f s  (%.1f img/s)\n",
           n_inputs, t1 - t0, n_inputs / (t1 - t0));
    return 0;
}
