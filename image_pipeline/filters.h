#ifndef FILTERS_H
#define FILTERS_H

#include "ppm.h"

/* All filters read from src and write to dst.
 * src and dst must have identical dimensions and both be pre-allocated.
 * Never pass the same Image as both src and dst — reads and writes would alias.
 *
 * Border pixels (outermost row/column) are copied unchanged because a full
 * 3x3 kernel neighbourhood doesn't fit there.
 */

/* Full-image versions — used by stage1_serial */
void box_blur(const Image *src, Image *dst);
void sharpen (const Image *src, Image *dst);

/* Row-range versions — used by stage2+ for strip parallelism.
 * Processes rows [start_row, end_row). */
void box_blur_rows(const Image *src, Image *dst, int start_row, int end_row);
void sharpen_rows (const Image *src, Image *dst, int start_row, int end_row);

#endif
