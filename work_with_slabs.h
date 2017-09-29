#ifndef WORK_WITH_SLABS_H
#define WORK_WITH_SLABS_H

#include <cstdio>
#include <mutex>
#include <atomic>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <cassert>

void init_slab_allocation();

bool is_allocated_by_slab(void* ptr);
char *alloc_block_in_slab(size_t size);
void free_block_in_slab(char *ptr);
char *realloc_block_in_slab(char *ptr, size_t new_size);
size_t malloc_usable_size_in_slab(char *ptr);

#endif
