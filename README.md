# Parallel Image Processing Pipeline

A batch image processor built in C with pthreads that demonstrates two distinct kinds of parallelism: **data parallelism** (splitting one image across threads) and **pipeline parallelism** (processing multiple images simultaneously through a chain of stages).

Built as a learning project for a parallel programming course. The image format is intentionally simple (PPM) so the focus stays on the threading patterns, not file I/O.

---

## What is demonstrated

### Stage 1 — Serial baseline
A single-threaded program that loads a PPM, applies a box blur, and saves the result. Provides the baseline time to beat.

### Stage 2 — Data parallelism (strip decomposition)
The same blur split across N threads. Each thread owns a horizontal band of rows. Because the filter reads from `src` and writes to `dst`, threads write to completely disjoint memory regions — no mutex needed inside the filter loop. Speedup is measurable but flattens early because box blur is memory-bandwidth-bound.

### Stage 3 — Pipeline parallelism
A 4-stage producer/consumer pipeline processing a batch of images:
```
[load] → Q1 → [blur] → Q2 → [sharpen] → Q3 → [save]
```
Each stage runs on its own thread. Bounded queues (capacity 4) between stages absorb speed mismatches. This doesn't make any single image faster — it makes the *batch* faster by keeping all stages busy simultaneously.

---

## Project structure

```
image_pipeline/
├── ppm.c / ppm.h              # load/save P6 binary PPM files
├── filters.c / filters.h      # box_blur, sharpen (serial + row-range variants)
├── queue.c / queue.h          # bounded blocking ImageQueue (mutex + 2 cond vars)
├── timer.h                    # GET_TIME macro (gettimeofday-based)
├── stage1_serial.c            # single-threaded baseline
├── stage2_parallel.c          # strip-decomposed parallel blur
├── stage3_pipeline.c          # 4-stage producer/consumer pipeline
├── stage3_serial_batch.c      # serial equivalent of stage3 (for fair comparison)
├── gen_test_ppm.c             # generates a 1920x1080 test image (no ImageMagick needed)
├── convert_outputs.sh         # converts PPM outputs to PNG for viewing
└── Makefile
```

---

## Prerequisites

- **MSYS2** with the UCRT64 toolchain: [msys2.org](https://www.msys2.org)
- Inside the **MSYS2 UCRT64** terminal:
  ```bash
  pacman -S make mingw-w64-ucrt-x86_64-gcc
  ```
- **ImageMagick** (optional, for viewing results) — run in PowerShell:
  ```powershell
  winget install ImageMagick.ImageMagick
  ```

---

## Building

Open the **MSYS2 UCRT64** terminal, navigate to the project, and run:

```bash
cd /c/Users/<you>/Documents/ParallelImageProcessing/image_pipeline
make
```

This builds all programs at once.

---

## Running

### Generate a test image

```bash
./gen_test_ppm input.ppm
```

Creates a 1920×1080 PPM with a colour gradient, concentric circles, and diagonal stripes. The stripes make blur and sharpen effects clearly visible.

---

### Stage 1 — Serial baseline

```bash
./stage1_serial input.ppm output.ppm
```

Example output:
```
load:  0.0028 s  (1920 x 1080)
blur:  0.0257 s
save:  0.0027 s
```

The `blur` time is the number to compare against Stage 2.

---

### Stage 2 — Parallel blur (vary thread count to see speedup)

```bash
./stage2_parallel input.ppm output.ppm 1
./stage2_parallel input.ppm output.ppm 2
./stage2_parallel input.ppm output.ppm 4
./stage2_parallel input.ppm output.ppm 8
```

Example results:

| Threads | Blur time | Speedup |
|---------|-----------|---------|
| serial  | 0.0257 s  | 1.00×   |
| 2       | 0.0196 s  | 1.31×   |
| 4       | 0.0147 s  | 1.75×   |
| 8       | 0.0117 s  | 2.20×   |

Speedup is real but sub-linear because box blur is memory-bandwidth-bound — more threads saturate the RAM bus rather than the CPU.

**Correctness check** (outputs must be byte-for-byte identical):
```bash
./stage1_serial input.ppm serial_ref.ppm
./stage2_parallel input.ppm parallel_ref.ppm 4
md5sum serial_ref.ppm parallel_ref.ppm
```

---

### Stage 3 — Pipeline vs serial batch (20 images)

Create a batch of 20 test images:
```bash
mkdir -p batch
for i in $(seq -w 01 20); do cp input.ppm batch/input_$i.ppm; done
```

Run the serial batch (load + blur + sharpen + save, one image at a time):
```bash
mkdir -p serial_out
./stage3_serial_batch serial_out batch/*.ppm
```

Run the pipeline (same work, all 4 stages overlapped):
```bash
mkdir -p pipeline_out
./stage3_pipeline pipeline_out batch/*.ppm
```

Example results:

| Mode     | 20 images | Throughput | Speedup |
|----------|-----------|------------|---------|
| Serial   | 1.212 s   | 16.5 img/s | 1.00×   |
| Pipeline | 0.806 s   | 24.8 img/s | 1.50×   |

---

## Viewing results

After installing ImageMagick, convert any PPM to PNG:
```bash
magick output.ppm output.png
```

Or convert an entire output directory at once:
```bash
bash convert_outputs.sh pipeline_out
```

The PNG files can be opened with any image viewer.
