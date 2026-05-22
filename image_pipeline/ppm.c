#include "ppm.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

Image *image_alloc(int width, int height) {
    Image *img = malloc(sizeof(Image));
    if (!img) return NULL;
    img->data = malloc((size_t)width * height * 3);
    if (!img->data) { free(img); return NULL; }
    img->width  = width;
    img->height = height;
    return img;
}

void image_free(Image *img) {
    if (!img) return;
    free(img->data);
    free(img);
}

static void skip_ws_comments(FILE *f) {
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '#') {
            while ((c = fgetc(f)) != EOF && c != '\n');
        } else if (!isspace(c)) {
            ungetc(c, f);
            return;
        }
    }
}

Image *ppm_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "ppm_load: cannot open '%s'\n", path);
        return NULL;
    }

    char magic[2];
    if (fread(magic, 1, 2, f) != 2 || magic[0] != 'P' || magic[1] != '6') {
        fprintf(stderr, "ppm_load: '%s' is not a P6 PPM file\n", path);
        fclose(f);
        return NULL;
    }

    int width, height, maxval;
    skip_ws_comments(f); fscanf(f, "%d", &width);
    skip_ws_comments(f); fscanf(f, "%d", &height);
    skip_ws_comments(f); fscanf(f, "%d", &maxval);

    if (width <= 0 || height <= 0 || maxval != 255) {
        fprintf(stderr, "ppm_load: '%s': bad dimensions or unsupported maxval %d\n", path, maxval);
        fclose(f);
        return NULL;
    }

    fgetc(f); /* consume the single whitespace byte separating header from binary data */

    Image *img = image_alloc(width, height);
    if (!img) {
        fprintf(stderr, "ppm_load: out of memory\n");
        fclose(f);
        return NULL;
    }

    size_t nbytes = (size_t)width * height * 3;
    if (fread(img->data, 1, nbytes, f) != nbytes) {
        fprintf(stderr, "ppm_load: '%s' is truncated\n", path);
        image_free(img);
        fclose(f);
        return NULL;
    }

    fclose(f);
    return img;
}

int ppm_save(const Image *img, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "ppm_save: cannot open '%s'\n", path);
        return 1;
    }

    fprintf(f, "P6\n%d %d\n255\n", img->width, img->height);

    size_t nbytes = (size_t)img->width * img->height * 3;
    if (fwrite(img->data, 1, nbytes, f) != nbytes) {
        fprintf(stderr, "ppm_save: write error on '%s'\n", path);
        fclose(f);
        return 1;
    }

    fclose(f);
    return 0;
}
