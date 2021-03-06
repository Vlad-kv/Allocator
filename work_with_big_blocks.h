#ifndef WORK_WITH_BIG_BLOCKS_H
#define WORK_WITH_BIG_BLOCKS_H

#include <cstdio>
#include "debug.h"

char *alloc_big_block(size_t size);
void free_big_block(char *ptr);
char *realloc_big_block(char *ptr, size_t new_size);
size_t malloc_usable_size_big_block(char *ptr);

namespace big_blocks {
	char *get_block(char *ptr);
	size_t get_size(char *ptr);
	void set_block(char *ptr, char *block);
	void set_size(char *ptr, size_t size);
}

#endif
