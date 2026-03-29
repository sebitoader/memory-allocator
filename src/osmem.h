/* SPDX-License-Identifier: BSD-3-Clause */

#pragma once

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include "printf.h"
#include "block_meta.h"

#define MMAP_THRESHOLD (128 * 1024)  // 128 KB
#define MAP_ANONYMOUS 0x20
#define PREALLOC_SIZE (128 * 1024) // 128 KB
extern struct block_meta *mmap_base_head;
extern struct block_meta *sbrk_base_head;

size_t align(size_t size);
void split_block(struct block_meta **block, size_t size);
void *init_preallocated_heap();
void coalesce_free_blocks();

void *os_malloc(size_t size);
void os_free(void *ptr);
void *os_calloc(size_t nmemb, size_t size);
void *os_realloc(void *ptr, size_t size);
