# Memory Allocator

A custom dynamic memory allocator for Linux implementing `os_malloc`,
`os_free`, `os_calloc`, and `os_realloc` directly on top of `brk`/`sbrk`
and `mmap` system calls, with no dependency on libc's allocator.

## Performance

Benchmarked on 1000 consecutive 64-byte allocations:

| Metric | This allocator | Naive (sbrk per call) |
|--------|---------------|----------------------|
| `brk()` syscalls | **1** | 1000 |

Block reuse after free incurs **zero additional syscalls** — freed blocks
are reclaimed from the free list without touching the heap.

## Implementation Highlights

### Heap Preallocation
On the first small allocation, 128KB is reserved upfront with a single
`sbrk` call. All subsequent small allocations split from this block
without any further syscalls.

### Best-Fit Block Selection
Every allocation scans the full free list and picks the smallest block
that satisfies the request, minimising wasted space compared to
first-fit strategies.

### Block Splitting
Oversized free blocks are split into an allocated block and a smaller
free remainder, keeping the leftover memory available for future use.

### Block Coalescing
On every `os_free`, adjacent free blocks are merged into a single
larger block to prevent external fragmentation from accumulating.

### Dual Allocation Strategy
- Below `MMAP_THRESHOLD` (128KB): heap-managed via `sbrk`, with
  reuse and coalescing
- At or above `MMAP_THRESHOLD`: individual `mmap` allocations,
  released immediately on free with `munmap`

## Block Metadata

Each allocation is prefixed with a `block_meta` header:
```c
struct block_meta {
    size_t size;    // aligned payload size
    int status;     // STATUS_FREE, STATUS_ALLOC, or STATUS_MAPPED
    struct block_meta *prev;
    struct block_meta *next;
};
```

The returned pointer points past the header to the usable payload.

## Building
```bash
cd src/
make
```

Produces `libosmem.so`, linkable into any program as a drop-in allocator.

## Technologies

C · Linux System Calls (`brk`/`sbrk`, `mmap`, `munmap`) ·
Doubly-Linked Lists · Memory Alignment · x86-64 Linux