#include "osmem.h"
#include <stdio.h>
#include <time.h>
#include <unistd.h>

int main(void) {
    int N = 1000;
    void *ptrs[1000];
    int brk_count = 0;
    struct timespec t1, t2;

    // --- MY ALLOCATOR ---
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (int i = 0; i < N; i++) {
        void *prev = sbrk(0);
        ptrs[i] = os_malloc(64);
        if (sbrk(0) != prev) brk_count++;
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    long elapsed = (t2.tv_sec - t1.tv_sec) * 1000000 +
                   (t2.tv_nsec - t1.tv_nsec) / 1000;

    printf("=== YOUR ALLOCATOR: %d x 64 byte allocs ===\n", N);
    printf("brk() calls : %d\n", brk_count);
    printf("Time        : %ld us\n\n", elapsed);

    for (int i = 0; i < N; i++) os_free(ptrs[i]);

    // --- NAIVE: one sbrk per alloc ---
    brk_count = 0;
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (int i = 0; i < N; i++) {
        sbrk(64);
        brk_count++;
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    elapsed = (t2.tv_sec - t1.tv_sec) * 1000000 +
              (t2.tv_nsec - t1.tv_nsec) / 1000;

    printf("=== NAIVE: %d x 64 byte allocs ===\n", N);
    printf("brk() calls : %d\n", brk_count);
    printf("Time        : %ld us\n", elapsed);

    return 0;
}