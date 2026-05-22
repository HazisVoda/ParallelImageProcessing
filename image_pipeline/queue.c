#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

ImageQueue *queue_create(int capacity) {
    ImageQueue *q = malloc(sizeof(ImageQueue));
    if (!q) return NULL;
    q->buf = malloc((size_t)capacity * sizeof(Image *));
    if (!q->buf) { free(q); return NULL; }
    q->capacity = capacity;
    q->head = q->tail = q->count = q->done = 0;
    pthread_mutex_init(&q->mutex,     NULL);
    pthread_cond_init (&q->not_empty, NULL);
    pthread_cond_init (&q->not_full,  NULL);
    return q;
}

void queue_destroy(ImageQueue *q) {
    if (!q) return;
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy (&q->not_empty);
    pthread_cond_destroy (&q->not_full);
    free(q->buf);
    free(q);
}

void queue_put(ImageQueue *q, Image *img) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == q->capacity)
        pthread_cond_wait(&q->not_full, &q->mutex);
    q->buf[q->tail] = img;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

Image *queue_take(ImageQueue *q) {
    pthread_mutex_lock(&q->mutex);
    /* wait until there is an item OR the producer is done */
    while (q->count == 0 && !q->done)
        pthread_cond_wait(&q->not_empty, &q->mutex);
    if (q->count == 0) {        /* done=1 and queue is empty — signal end-of-stream */
        pthread_mutex_unlock(&q->mutex);
        return NULL;
    }
    Image *img = q->buf[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return img;
}

void queue_finish(ImageQueue *q) {
    pthread_mutex_lock(&q->mutex);
    q->done = 1;
    pthread_cond_broadcast(&q->not_empty); /* wake all waiting consumers */
    pthread_mutex_unlock(&q->mutex);
}
