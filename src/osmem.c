// SPDX-License-Identifier: BSD-3-Clause

#include "osmem.h"
#include "block_meta.h"

struct block_meta *sbrk_base_head;
struct block_meta *mmap_base_head;

size_t align(size_t size)
{
	return ((size + 7) / 8) * 8;
}

struct block_meta *find_best(size_t size)
{
	struct block_meta *temp = sbrk_base_head;
	struct block_meta *best_fit = NULL;

	while (temp) {
		if (temp->status == STATUS_FREE && temp->size >= size) {
			if (!best_fit || temp->size < best_fit->size)
				best_fit = temp;
		}
		temp = temp->next;
	}
	return best_fit;
}

void split_block(struct block_meta **block, size_t size)
{
	if ((*block)->size >= size + sizeof(struct block_meta) + 8) {
		struct block_meta *new_block = (struct block_meta *)((char *)(*block) + sizeof(struct block_meta) + size);

		new_block->size = (*block)->size - size - sizeof(struct block_meta);
		new_block->status = STATUS_FREE;
		new_block->next = (*block)->next;
		new_block->prev = (*block);
		if ((*block)->next)
			(*block)->next->prev = new_block;
		(*block)->next = new_block;
		(*block)->size = size;
	}
}

void *init_preallocated_heap(void)
{
	sbrk_base_head = sbrk(PREALLOC_SIZE);
	DIE(sbrk_base_head == (void *)-1, "sbrk failed");
	sbrk_base_head->size = PREALLOC_SIZE - sizeof(struct block_meta);
	sbrk_base_head->status = STATUS_FREE;
	sbrk_base_head->next = NULL;
	sbrk_base_head->prev = NULL;
	return sbrk_base_head;
}

void coalesce_free_blocks(void)
{
	struct block_meta *temp = sbrk_base_head;

	while (temp && temp->next) {
		if (temp->status == STATUS_FREE && temp->next->status == STATUS_FREE) {
			struct block_meta *next_block = temp->next;

			temp->size += next_block->size + sizeof(struct block_meta);
			temp->next = next_block->next;
			if (next_block->next)
				next_block->next->prev = temp;
		} else {
			temp = temp->next;
		}
	}
}

void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	size_t a_size = align(size);

	if (a_size + sizeof(struct block_meta) >= MMAP_THRESHOLD) {
		struct block_meta *block = mmap(NULL, a_size + sizeof(struct block_meta),
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(block == MAP_FAILED, "mmap failed");
		block->size = a_size;
		block->status = STATUS_MAPPED;
		block->next = mmap_base_head;
		if (mmap_base_head)
			mmap_base_head->prev = block;
		mmap_base_head = block;
		return (void *)(block + 1);
	}
	if (!sbrk_base_head) {
		sbrk_base_head = init_preallocated_heap();
		if (!sbrk_base_head)
			return NULL;
		sbrk_base_head->status = STATUS_ALLOC;
		split_block(&sbrk_base_head, a_size);
		return (void *)(sbrk_base_head + 1);
	}
	struct block_meta *block = find_best(a_size);

	if (block) {
		block->status = STATUS_ALLOC;
		split_block(&block, a_size);
		return (void *)(block + 1);
	}
	struct block_meta *last = sbrk_base_head;

	while (last && last->next)
		last = last->next;
	if (last && last->status == STATUS_FREE) {
		void *p = sbrk(a_size - last->size);

		DIE(p == (void *)-1, "sbrk failed.");
		last->size = a_size;
		last->status = STATUS_ALLOC;
		return (void *)(last + 1);
	}

	block = sbrk(a_size + sizeof(struct block_meta));
	DIE(block == (void *)-1, "sbrk failed");
	block->size = a_size;
	block->status = STATUS_ALLOC;
	last = sbrk_base_head;
	while (last && last->next)
		last = last->next;
	if (last) {
		last->next = block;
		block->prev = last;
	}
	return (void *)(block + 1);
}

void os_free(void *ptr)
{
	if (!ptr)
		return;
	struct block_meta *block = (struct block_meta *)ptr - 1;
	struct block_meta *temp = mmap_base_head;

	while (temp) {
		if (temp == block) {
			if (block == mmap_base_head) {
				mmap_base_head = mmap_base_head->next;
				int result = munmap(block, block->size + sizeof(struct block_meta));

				DIE(result == -1, "munmap failed");
				return;
			}
			if (temp->next)
				temp->next->prev = temp->prev;
			if (temp->prev)
				temp->prev->next = temp->next;
			int result = munmap(block, block->size + sizeof(struct block_meta));

			DIE(result == -1, "munmap failed");
			return;
		}
		temp = temp->next;
	}
	block->status = STATUS_FREE;
	coalesce_free_blocks();
}

void *os_calloc(size_t nmemb, size_t size)
{
	if (nmemb == 0 || size == 0)
		return NULL;
	size_t total_size = nmemb * size;
	size_t a_size = align(total_size);
	size_t page_size = getpagesize();
	void *ptr;

	if (a_size + sizeof(struct block_meta) < page_size) {
		ptr = os_malloc(a_size);
	} else {
		struct block_meta *block = mmap(NULL, a_size + sizeof(struct block_meta),
			PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		DIE(block == MAP_FAILED, "mmap failed");
		block->size = a_size;
		block->status = STATUS_MAPPED;
		block->next = mmap_base_head;
		if (mmap_base_head)
			mmap_base_head->prev = block;
		mmap_base_head = block;
		ptr = (void *)(block + 1);
	}
	if (ptr)
		memset(ptr, 0, a_size);

	return ptr;
}

void *os_realloc(void *ptr, size_t size)
{
	if (ptr == NULL)
		return os_malloc(size);

	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	struct block_meta *block = (struct block_meta *)ptr - 1;
	size_t a_size = align(size);

	if (block->status == STATUS_FREE)
		return NULL;

	if (block->status == STATUS_MAPPED) {
		void *p = os_malloc(a_size);

		if (!p)
			return NULL;

		memcpy(p, ptr, a_size);
		os_free(ptr);
		return p;
	}

	if (a_size <= block->size) {
		split_block(&block, a_size);
		return (void *)(block + 1);
	}

	struct block_meta *current = block;
	size_t total_size = block->size;

	while (current->next && current->next->status == STATUS_FREE) {
		total_size += current->next->size + sizeof(struct block_meta);
		current = current->next;

		if (total_size >= a_size) {
			block->size = total_size;
			block->next = current->next;
			if (block->next)
				block->next->prev = block;

			split_block(&block, a_size);
			return (void *)(block + 1);
		}
	}

	if (block->next == NULL && total_size < a_size) {
		void *p = sbrk(a_size - block->size);

		DIE(p == (void *)-1, "sbrk failed.");
		block->size = a_size;
		return (void *)(block + 1);
	}

	void *p = os_malloc(a_size);

	if (!p)
		return NULL;

	memcpy(p, ptr, block->size);
	os_free(ptr);
	return p;
}
