#ifndef WORK_WITH_SLABS_H
#define WORK_WITH_SLABS_H

#include <cstdio>
#include "debug.h"

char *alloc_block_in_slab(size_t size);

void free_block_in_slab(char *ptr);

char *realloc_block_in_slab(char *ptr, size_t new_size);

#endif
