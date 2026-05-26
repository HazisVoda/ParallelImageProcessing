# Parallel Image Processing Pipeline

A C/pthreads implementation demonstrating two distinct kinds of parallelism on a real-world image processing workload. This report walks through what the project does, how it's structured, what the benchmarks show, and what the trade-offs are.

---

## 1. What this project is

A batch image processor written in C using POSIX threads. It loads PPM images, applies image filters (box blur and sharpen), and saves the results. The interesting part isn't the image processing itself — that's just a vehicle. The interesting part is that the project demonstrates **two fundamentally different ways of making programs faster with threads**, and lets us measure both:

- **Data parallelism** — split *one* image across multiple threads, each working on its own horizontal strip of pixels. This is the same pattern used in the parallel pi estimator from class, but in 2D.
- **Pipeline parallelism** — chain multiple filter stages together (load → blur → sharpen → save) and run them simultaneously on different images, with bounded queues between stages. This is the producer/consumer pattern, just extended to a longer assembly line.

Both kinds of parallelism solve different problems. Data parallelism makes a single image process faster. Pipeline parallelism makes a *batch* of images process faster, without making any individual image faster. The project implements and measures both, so you can see the contrast directly.

The image format is PPM (Portable PixMap, P6 binary variant) — a deliberately boring uncompressed format with a ~5-line header followed by raw RGB bytes. Using PPM means zero time spent fighting a JPEG or PNG library; all the engineering time goes to threading. PNG outputs are produced by converting with ImageMagick afterward.

---

## 2. Project structure

```
image_pipeline/
├── ppm.c / ppm.h              # PPM load/save, Image struct, PIXEL macro
├── filters.c / filters.h      # box_blur, sharpen (full + row-range variants)
├── queue.c / queue.h           # bounded blocking queue of Image*
├── timer.h                     # GET_TIME macro (gettimeofday-based)
├── stage1_serial.c             # serial baseline: load → blur → save
├── stage2_parallel.c           # parallel blur with strip decomposition
├── stage3_pipeline.c           # 4-thread pipeline over a batch
├── stage3_serial_batch.c       # serial baseline for the batch (comparison)
├── gen_test_ppm.c              # generates a 1920×1080 synthetic test image
├── convert_outputs.sh          # converts output PPMs to PNG via ImageMagick
└── Makefile                    # builds all targets with -O2 -Wall -Wextra
```

| File | Lines | Purpose |
|---|---|---|
| `ppm.c` / `ppm.h` | ~165 | Image I/O — load and save P6 binary PPM |
| `filters.c` / `filters.h` | ~90 | Box blur and sharpen, with row-range variants |
| `queue.c` / `queue.h` | ~95 | Thread-safe bounded FIFO of `Image*` |
| `timer.h` | ~12 | `GET_TIME` macro using `gettimeofday` |
| `stage1_serial.c` | ~37 | Serial baseline (single image) |
| `stage2_parallel.c` | ~81 | Data-parallel blur (single image, N threads) |
| `stage3_pipeline.c` | ~129 | Pipeline-parallel batch processing |
| `stage3_serial_batch.c` | ~53 | Serial baseline for batch (comparison) |
| `gen_test_ppm.c` | ~41 | Synthetic test image generator |

The two filter functions exist as a pair: a full-image version (`box_blur`, `sharpen`) and a row-range version (`box_blur_rows`, `sharpen_rows`). The full-image versions are thin wrappers that just call the row-range version over the entire range. This means the serial program and the parallel program run *literally the same arithmetic code* on each pixel — there is no separate "serial" and "parallel" implementation of the math, only different ways of dispatching the work. That's by design, both for code reuse and for the byte-for-byte correctness it guarantees.

---

## 3. Why C and not Java?

The course covered both Java and pthreads, and either language could implement this project. C was the right choice here for a few reasons:

- **No hidden allocations.** Image buffers can be ~6 MB each; the pipeline holds several in flight at a time. C makes the memory cost completely visible, which keeps the project honest.
- **No GC pauses to muddy the timing.** Parallel speedup numbers are easier to interpret when there's no garbage collector occasionally pausing the world.
- **pthreads is what's actually underneath Java's `Thread`.** On Linux and macOS, every Java thread eventually becomes a `pthread_create` call. Working directly with the primitive layer makes the cost of synchronization visible in a way that Java's `synchronized` keyword hides.

The trade-off, of course, is that C makes you do everything by hand: mutex locking and unlocking with no auto-release on scope exit, manual `malloc`/`free`, manual cast through `void*` to pass arguments to thread functions. Forgetting `pthread_mutex_unlock` deadlocks the whole program silently. Forgetting `free` leaks memory the GC would have caught. That's the deal.

---

## 4. The Image type and pixel layout

```c
typedef struct {
    int      width;
    int      height;
    uint8_t *data;   // width * height * 3 bytes, interleaved RGB, row-major
} Image;

#define PIXEL(img, x, y) ((img)->data + ((y) * (img)->width + (x)) * 3)
```

A few decisions worth flagging:

- **Interleaved RGB** (R,G,B,R,G,B,...) instead of planar (all R, then all G, then all B). Interleaved matches the on-disk PPM layout exactly, so `fread` and `fwrite` are direct copies with no rearrangement.
- **`uint8_t` for pixel bytes.** One byte per channel, range 0–255. Makes the intent explicit; `unsigned char` would also work but `uint8_t` is clearer.
- **`int` for dimensions.** No real image hits the 2-billion-pixel limit of a 32-bit int. The careful detail is in the *multiplication*: anywhere the code computes the buffer size, it casts to `size_t` first — `(size_t)width * height * 3` — because `width * height` in plain `int` overflows around 46k × 46k. `size_t` is 64-bit on modern systems and avoids the trap.
- **Heap-allocated struct, owned by pointer.** `image_alloc` returns an `Image*`, `image_free` takes one. This matches the model needed for the pipeline stages, where images get passed between threads through queues.

The `PIXEL` macro hides the address arithmetic. Without it, every filter function would be cluttered with `img->data[(y * img->width + x) * 3 + c]` expressions. With it, you write `PIXEL(img, x, y)[0]` for red, `[1]` for green, `[2]` for blue.

---

## 5. Stage 1 — Serial baseline

The point of a baseline isn't to be impressive. It's to be a number that the parallel versions have to beat. Without it, "the parallel version is fast" means nothing.

`stage1_serial.c` does the simplest possible thing: load one PPM, apply box blur, save the result. It prints timing for each phase (load, blur, save) using the `GET_TIME` macro from `timer.h`.

The box blur itself is a 3×3 averaging filter. For each output pixel, sum the values of the 9 pixels in the 3×3 neighborhood of the corresponding input pixel and divide by 9. There's one design rule that matters: **read from `src`, write to `dst`, never in-place.** If you tried to blur in place, you'd be reading pixel values that have already been overwritten by earlier iterations of the loop, and the output would be wrong. The src/dst separation isn't just clean code — it turns out to be exactly what makes the filter trivially parallelizable in stage 2.

Border pixels (the outermost row and column) are copied through unchanged, because a 3×3 kernel can't be centered on a pixel that doesn't have neighbors on all sides. This is by design, documented in `filters.h`, and visible as a 1-pixel border in the output.

---

## 6. Stage 2 — Data parallelism with strip decomposition

`stage2_parallel.c` splits the image into N horizontal strips, one per thread. With 4 threads on a 1080-row image:

```
Thread 0: rows    0..269
Thread 1: rows  270..539
Thread 2: rows  540..809
Thread 3: rows  810..1079
```

When `height` doesn't divide evenly by `thread_count`, the last thread absorbs the remainder — the same "last thread does a little more" pattern from `pi_estimation_remainder.c` in the course.

The key insight: **no mutex is needed inside the filter loop.** Because each thread writes to disjoint rows of `dst` (no two threads write the same pixel), and the reads from `src` are all read-only (multiple threads reading the same memory is always safe), there's nothing to synchronize. Threads can blast through their strips without ever talking to each other. The only synchronization in the whole program is `pthread_create` at the start and `pthread_join` at the end.

This is much cleaner than the bank-account examples from class, where every increment needed a lock around it. The difference is the src/dst separation: by writing to a *different* buffer than we're reading from, we eliminate write-write conflicts entirely.

The thread function takes a `BlurArgs` struct (passed via `void*`) with pointers to src and dst, the start row, and the end row. No globals, no shared mutable state, no locks.

---

## 7. Stage 3 — Pipeline parallelism

`stage3_pipeline.c` is structurally different. Instead of splitting one image across threads, it chains multiple filters into stages and runs them concurrently on a stream of images:

```
[load thread] → Q1 → [blur thread] → Q2 → [sharpen thread] → Q3 → [save thread]
```

Four threads, three queues. Each queue holds at most 4 `Image*` pointers (bounded). When a queue is full, the upstream producer blocks until space opens up. When a queue is empty, the downstream consumer blocks until an image arrives. This is exactly the `ArrayBlockingQueue` pattern from Java, hand-rolled in C with a mutex and two condition variables per queue:

```c
typedef struct {
    Image          **buffer;
    int              capacity;
    int              count;
    int              head, tail;
    int              done;            // producer signals end-of-stream
    pthread_mutex_t  mutex;
    pthread_cond_t   not_empty;       // waited on by consumers
    pthread_cond_t   not_full;        // waited on by producers
} ImageQueue;
```

The `done` flag handles graceful shutdown. When the load thread finishes the last image, it calls `queue_finish()` on its output queue. From then on, `queue_take()` will drain whatever's still buffered and then start returning `NULL`. Each downstream thread sees the `NULL`, propagates the `done` signal to *its* output queue, and exits. The shutdown cascades cleanly from the front of the pipeline to the back without anyone hanging.

`queue_finish()` uses `pthread_cond_broadcast` (not `signal`) deliberately — if multiple consumers are blocked waiting on the same queue, signal would wake only one of them and the others would deadlock. Broadcast wakes them all; whichever one runs first sees the `done` flag and the others get the same signal on their next wait.

Stage 3 actually does *two* filters (blur then sharpen) where stage 1 only does one. That's important for the comparison story — it means stage 1 timing and stage 3 timing aren't directly comparable. The right comparison for stage 3 is `stage3_serial_batch`, which runs the same load → blur → sharpen → save sequence on the same batch but does it sequentially, one image at a time.

---

## 8. Benchmarks

All measurements taken on a 1920×1080 synthetic PPM (~5.9 MB). The "batch" case processes 20 copies of that same image.

### Stage 1 — Serial baseline (single image)

```
load:  0.0246 s
blur:  0.0272 s
save:  0.0028 s
```

The blur is the dominant phase. Load and save are essentially fixed costs.

### Stage 2 — Parallel blur (single image, varying thread count)

| Threads | Load | Blur | Save | Speedup vs. serial blur |
|---------|------|------|------|-------------------------|
| 1 (serial) | 0.0246 | 0.0272 | 0.0028 | 1.00× |
| 2 | 0.0027 | 0.0219 | 0.0028 | 1.24× |
| 4 | 0.0030 | 0.0146 | 0.0035 | **1.86×** |
| 8 | 0.0036 | 0.0175 | 0.0025 | 1.55× |

A few things stand out:

- **4 threads gives the best blur time.** 8 threads is *slower* than 4. This is a real and slightly humbling result — more parallelism isn't always better. With strips this small (135 rows per thread at 8 threads), thread management overhead and cache contention start to cost more than they save. The test machine almost certainly has 4 physical cores; threads 5 through 8 are competing for the same cores via hyperthreading, which helps some workloads but hurts cache-sensitive ones like blur.
- **Speedup peaks at 1.86×, not 4×.** The 4-thread "ideal" speedup would be 4×, but real-world speedup is limited by memory bandwidth (every pixel has to be read from RAM), thread startup cost, and the small amount of fixed work outside the parallel region. 1.86× on 4 threads is a respectable result for a memory-bound workload.
- **Load times look anomalously low after stage 1.** Stage 1's load took 24.6ms; stage 2 runs show 2.7–3.6ms. This is OS file caching at work — once `input.ppm` has been read once, it sits in the kernel page cache and subsequent reads are essentially memory copies. The "real" first-time load cost is the 24.6ms number; everything after that is cache.

### Stage 3 — Pipeline vs. serial batch (20 images, load → blur → sharpen → save)

```
serial:    20 images in 1.762 s   (11.4 img/s)
pipeline:  20 images in 0.825 s   (24.2 img/s)
```

**Speedup: 2.14×.**

This is the pipeline-parallelism story. Notice what it does and doesn't do:

- It doesn't make any single image's processing faster. Each image still goes through load, blur, sharpen, save in sequence.
- It makes the *batch* faster by keeping all four stages busy at once. While the save thread is writing image 5 to disk, the sharpen thread is sharpening image 6, the blur thread is blurring image 7, and the load thread is reading image 8. Four images are in flight simultaneously.
- The theoretical maximum speedup is 4× (four stages running in parallel). Achieving 2.14× means the four stages are not equally expensive: the slowest stage (probably blur, given the per-image numbers) is the bottleneck, and the faster stages spend time blocked waiting.

This nicely demonstrates Amdahl's-Law-like behavior at the pipeline level: pipeline throughput is gated by the slowest stage, no matter how fast the other stages are.

### What the two kinds of parallelism are doing differently

| | Stage 2 (data) | Stage 3 (pipeline) |
|---|---|---|
| Speeds up | One image | A batch of images |
| Threads work on | Different rows of the same image | Different images at the same stage |
| Synchronization | None inside the work | Mutex + 2 condvars per queue |
| Limited by | Memory bandwidth, core count | Slowest pipeline stage |
| Best speedup observed | 1.86× | 2.14× |

The fact that they're attacking different bottlenecks is the whole point. In a production image processor (think Lightroom, Instagram's upload pipeline), you'd do both at once: each stage uses data-parallel filters internally, and the stages themselves run in parallel as a pipeline. That hybrid version was planned as `stage4_hybrid.c` and skipped due to time constraints.

---

## 9. Build and run

The project requires a POSIX-compatible toolchain. On Linux and macOS this works out of the box. On Windows it requires MSYS2/MinGW, Cygwin, or WSL — `timer.h` uses `gettimeofday` from `<sys/time.h>`, which MSVC does not provide.

```bash
# Build everything
make

# Generate the synthetic test image (if needed)
./gen_test_ppm input.ppm

# Stage 1 — serial blur on one image
./stage1_serial input.ppm out1.ppm

# Stage 2 — parallel blur with N threads
./stage2_parallel input.ppm out2.ppm 4

# Stage 3 — pipelined batch (output dir + input files)
./stage3_pipeline pipeline_out batch/input_*.ppm

# Stage 3 serial baseline (for comparison)
./stage3_serial_batch serial_out batch/input_*.ppm

# Clean build artifacts
make clean
```

Compilation flags: `-Wall -Wextra -O2 -g -lpthread -lm`. No warnings during development.

To visually inspect output, convert PPMs to PNG with ImageMagick:

```bash
./convert_outputs.sh pipeline_out/
```

---

## 10. Known limitations

A handful of things worth flagging openly:

- **`stage4_hybrid.c` not built.** The Makefile has a rule for it but the source was never written. This would have combined data parallelism (parallel blur within a stage) with pipeline parallelism (parallel stages) and is the natural next step.
- **Byte-for-byte correctness not formally verified.** Because `stage2_parallel` and `stage1_serial` both ultimately call `box_blur_rows()` over the same row ranges, the outputs *should* be bit-exact. But no `diff` or `fc /b` comparison has been recorded in the test artifacts. Running `diff out1.ppm out2.ppm` would close this gap in seconds.
- **`make clean` is incomplete.** It removes the main stage executables but misses `stage3_serial_batch` and `gen_test_ppm`. A trivial fix that didn't make it into the submitted version.
- **The 20-image batch is 20 copies of the same image.** This is fine for timing measurement (each image takes the same time, which makes pipeline analysis cleaner), but it doesn't exercise any input-dependent behavior. A more thorough test would use 20 different images.
- **Pipeline queue capacity is hardcoded at 4.** With 4-slot queues and ~6 MB images, peak memory is bounded at roughly 100 MB. Larger queues would let fast stages run further ahead but use more memory; smaller queues would force tighter coupling between stages. 4 was picked as a reasonable midpoint, not by tuning.
- **No NUMA awareness, no thread pinning, no SIMD.** This is straightforward portable pthreads code. A production implementation would do all three (pin threads to specific cores to avoid cache thrashing, use AVX intrinsics for the per-pixel math, allocate memory on the same NUMA node as the thread that uses it). Out of scope here.

---

## 11. What this project demonstrates

Stripping away the image-processing context, the project is a working demonstration of:

1. **Strip decomposition for data parallelism** — exactly the block-distribution pattern from the parallel pi estimator, applied to 2D.
2. **Bounded blocking queues** built from a mutex and two condition variables — the same primitive that `ArrayBlockingQueue` provides in Java, hand-rolled in C.
3. **Multi-stage pipeline parallelism with clean shutdown** — the producer/consumer pattern extended into an assembly line, with `done` flags cascading from front to back.
4. **Real measured speedup numbers**, including the educational result that 8 threads is sometimes slower than 4 because of cache and core contention.
5. **Correctness through structure rather than locks** — the strip filter needs no mutex because src/dst separation eliminates write conflicts.

Bigger picture: the project simulates what tools like ImageMagick's batch mode, Adobe Lightroom's import pipeline, or Instagram's upload service do internally. They use both kinds of parallelism — data parallelism per image, pipeline parallelism across the batch — and the project shows each one in isolation so the contrast is visible.

The same patterns generalize beyond images. A web server's request handler chain is a pipeline. An audio processing rack is a pipeline. A video transcoder uses data parallelism within a frame and pipeline parallelism across frames. The vocabulary is the same; only the data type in the queue changes.