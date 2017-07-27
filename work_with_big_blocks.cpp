#include "work_with_big_blocks.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

#include <sys/mman.h>
#include <errno.h>

#include "constants.h"
#include "debug.h"

char *alloc_big_block(size_t size) {
	size = size + (PAGE_SIZE - size % PAGE_SIZE) % PAGE_SIZE + PAGE_SIZE;

	char *block = (char*)mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (block == MAP_FAILED) {
		print("MAP_FAILED in alloc_big_block\n");
		return nullptr;
	}
	char *res = block + PAGE_SIZE;

	*(char**)(res - 3 * sizeof(char*)) = block;
	*(size_t*)(res - 2 * sizeof(char*)) = size;
	
	return res;
}

void free_big_block(char *ptr) {
	char *block = *(char**)(ptr - 3 * sizeof(char*));
	size_t size = *(size_t*)(ptr - 2 * sizeof(char*));
	int error = munmap(block, size);

	my_assert(error == 0, "error in munmup in free_big_block: ", errno);
}

char *realloc_big_block(char *ptr, size_t new_size) {
	char *block = *(char**)(ptr - 3 * sizeof(char*));
	size_t size = *(size_t*)(ptr - 2 * sizeof(char*));

	size_t old_size = size - (ptr - block);

	if (new_size <= MAX_SIZE_TO_ALLOC_IN_SLAB) {
		char *res = alloc_block_in_slab(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		std::memcpy(res, ptr, std::min(new_size, old_size));
		int error = munmap(block, size);

		my_assert(error == 0, "error in munmup in free_big_block");
		return res;
	}
	if (new_size <= MAX_SIZE_TO_ALLOC_IN_CLUSTERS) {
		char *res = alloc_block_in_cluster(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		std::memcpy(res, ptr, std::min(new_size, old_size));
		int error = munmap(block, size);
		my_assert(error == 0, "error in munmup in free_big_block");
		return res;
	}
	// TODO - понавтыкать if-ов чтобы не alloc_big_block каждый раз
	char *res = alloc_big_block(new_size);
	if (res == nullptr) {
		return nullptr;
	}
	std::memcpy(res, ptr, std::min(new_size, old_size));
	int error = munmap(block, size);
	my_assert(error == 0, "error in munmup in free_big_block");
	return res;
}
