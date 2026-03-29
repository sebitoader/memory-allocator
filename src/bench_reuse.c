#include "osmem.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    int N = 500;
    int brk_calls = 0;
    struct timespec t1, t2;

    // Warm up heap
    void *warm = os_malloc(64);
    os_free(warm);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (int i = 0; i < N; i++) {
        void *prev = sbrk(0);
        void *p = os_malloc(128);
        if (sbrk(0) != prev) brk_calls++;

        os_free(p);

        prev = sbrk(0);
        void *q = os_malloc(64);
        if (sbrk(0) != prev) brk_calls++;
        os_free(q);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    long elapsed = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    printf("=== BLOCK REUSE: %d alloc/free/realloc cycles ===\n", N);
    printf("Extra brk() calls : %d\n", brk_calls);
    printf("Time              : %ld us\n", elapsed);

    return 0;
}