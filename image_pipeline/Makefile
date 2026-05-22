CC     = gcc
CFLAGS = -Wall -Wextra -O2 -g
LFLAGS = -lpthread -lm

COMMON = ppm.c filters.c
QUEUE  = queue.c

all: stage1_serial stage2_parallel stage3_pipeline stage3_serial_batch gen_test_ppm

gen_test_ppm: gen_test_ppm.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

stage1_serial: stage1_serial.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

stage2_parallel: stage2_parallel.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

stage3_pipeline: stage3_pipeline.c $(COMMON) $(QUEUE)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

stage3_serial_batch: stage3_serial_batch.c $(COMMON)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

stage4_hybrid: stage4_hybrid.c $(COMMON) $(QUEUE)
	$(CC) $(CFLAGS) $^ -o $@ $(LFLAGS)

clean:
	rm -f stage1_serial stage2_parallel stage3_pipeline stage4_hybrid *.o

.PHONY: all clean
