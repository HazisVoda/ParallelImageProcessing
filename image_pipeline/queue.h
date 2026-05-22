#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include "ppm.h"

/* Bounded blocking queue of Image pointers.
 *
 * Producers call queue_put() — blocks when full.
 * Consumers call queue_take() — blocks when empty, returns NULL when the
 *   producer calls queue_finish() and the queue drains.
 * Exactly one queue_finish() call per queue, by whichever thread is done
 *   producing. Downstream stages propagate finish to their own output queue
 *   after their input queue returns NULL.
 */
typedef struct {
    Image         **buf;
    int             capacity;
    int             head;       /* next read index  */
    int             tail;       /* next write index */
    int             count;
    int             done;       /* set by producer when no more items */
    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} ImageQueue;

ImageQueue *queue_create (int capacity);
void        queue_destroy(ImageQueue *q);
void        queue_put    (ImageQueue *q, Image *img);
Image      *queue_take   (ImageQueue *q);          /* returns NULL when done+empty */
void        queue_finish (ImageQueue *q);           /* signal no more items */

#endif
