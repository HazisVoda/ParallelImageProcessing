#ifndef TIMER_H
#define TIMER_H

#include <sys/time.h>

#define GET_TIME(t) do {            \
    struct timeval _tv;             \
    gettimeofday(&_tv, NULL);       \
    (t) = _tv.tv_sec + _tv.tv_usec * 1e-6; \
} while (0)

#endif
