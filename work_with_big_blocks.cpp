#include "work_with_big_blocks.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

#include <sys/mman.h>
#include <errno.h>
#include <atomic>

#include "constants.h"
#include "debug.h"

using namespace std;

namespace big_blocks {
	char *get_block(char *ptr) {
		return (*(atomic<char*>*)(ptr - 3 * sizeof(char*))).load();
	}
	size_t get_size(char *ptr) {
		return (*(atomic<size_t>*)(ptr - 2 * sizeof(char*))).load();
	}
	void set_block(char *ptr, char *block) {
		(*(atomic<char*>*)(ptr - 3 * sizeof(char*))).store(block);
	}
	void set_size(char *ptr, size_t size) {
		(*(atomic<size_t>*)(ptr - 2 * sizeof(char*))).store(size);
	}
}
using namespace big_blocks;

char *alloc_big_block(size_t size) {
	size = size + (PAGE_SIZE - size % PAGE_SIZE) % PAGE_SIZE + PAGE_SIZE;

	char *block = (char*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (block == MAP_FAILED) {
		// print("MAP_FAILED in alloc_big_block\n");
		return nullptr;
	}
	char *res = block + PAGE_SIZE;

	set_block(res, block);
	set_size(res, size);
	
	return res;
}

void free_big_block(char *ptr) {
	int error = munmap(get_block(ptr), get_size(ptr));
	my_assert(error == 0, "error in munmup in free_big_block: ", errno);
}

char *realloc_big_block(char *ptr, size_t new_size) {
	char *block = get_block(ptr);
	size_t size = get_size(ptr);

	size_t old_size = size - (ptr - block);

	if (new_size <= MAX_SIZE_TO_ALLOC_IN_SLAB) {
		char *res = alloc_block_in_slab(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		memcpy(res, ptr, min(new_size, old_size));
		int error = munmap(block, size);

		my_assert(error == 0, "error in munmup in realloc_big_block");
		return res;
	}
	if (new_size <= MAX_SIZE_TO_ALLOC_IN_CLUSTERS) {
		char *res = alloc_block_in_cluster(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		memcpy(res, ptr, min(new_size, old_size));
		int error = munmap(block, size);
		my_assert(error == 0, "error in munmup in realloc_big_block");
		return res;
	}

	new_size += (PAGE_SIZE - new_size % PAGE_SIZE) % PAGE_SIZE;
	if (new_size == old_size) {
		return ptr;
	}

	if (new_size < old_size) {
		int error = munmap(ptr + new_size, old_size - new_size);
		my_assert(error == 0, "error in munmup in realloc_big_block (new_size < old_size) : ", errno);
		set_size(ptr, size + old_size - new_size);
		return ptr;
	}
	
	char *res = alloc_big_block(new_size);
	if (res == nullptr) {
		return nullptr;
	}
	memcpy(res, ptr, min(new_size, old_size));
	int error = munmap(block, size);
	my_assert(error == 0, "error in munmup in realloc_big_block : ", errno);
	return res;
}

size_t malloc_usable_size_big_block(char *ptr) {
	return get_size(ptr) - (ptr - get_block(ptr));
}
