#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <mutex>
#include <atomic>
#include <sys/mman.h>

#include "debug.h"

#include "work_with_big_blocks.h"
#include "constants.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

typedef unsigned long long ull;
using namespace std;

// recursive_mutex test_mutex;

extern thread_local int num_alive_writers;

atomic_bool is_initialized;
atomic_bool is_constructors_begin_to_executing;
mutex init_mutex;

struct test_struct {
	test_struct() {
		is_constructors_begin_to_executing.store(true);
		is_initialized.store(false);

		// print_to_console("in test_struct\n");
	}
	~test_struct() {
		// print_to_console("in ~test_struct\n");
	}
	int w = 1;
};
test_struct test;

void initialize() {
	// print_to_console("in initialize\n");

	init_global_clusters_data();
	init_slab_allocation();
}

void check_init() {
	if (is_initialized.load() == true) {
		return;
	}
	lock_guard<mutex> lg(init_mutex);

	if (is_initialized.load() == true) {
		return;
	}
	is_initialized.store(true);
	initialize();
}

extern "C" void *malloc(size_t size) {
	// print_to_console("in malloc : ", size, "\n");
	size = max(size, (size_t)8);

	if (is_constructors_begin_to_executing.load() == false) {
		// print_to_console("mmaping : ", size, "\n");
    	char *res = alloc_big_block(size);
    	if (res == nullptr) { // debug
    		fatal_error("return nullptr in malloc !!!\n");
    	}
    	return res;
    }
	// lock_guard<recursive_mutex> lg(test_mutex);

	if (num_alive_writers > 0) {
		// print_to_console("malloc for print : ", size, "\n");
		char* res = alloc_big_block(size);
		if (res == nullptr) {
			WRITE("  Error in malloc for print : too small size of buffer\n");
			exit(1);
		}
		return res;
	}

	if (size == (size_t)-1) {   // debug
		return dlsym(RTLD_NEXT, "malloc");
	}
	if (size == (size_t)-2) {
		return dlsym(RTLD_NEXT, "free");
	}
	if (size == (size_t)-3) {
		return dlsym(RTLD_NEXT, "calloc");
	}
	if (size == (size_t)-4) {
		return dlsym(RTLD_NEXT, "realloc");
	}
	if (size == (size_t)-5) {
		return dlsym(RTLD_NEXT, "posix_memalign");
	}

	print("malloc catched: ", size, "\n");

    check_init();
    
	if ((size == 0) || (size > MAX_SIZE_TO_ALLOC)) {
		return nullptr;
	}

	char *res = alloc_if_clusters_not_fully_initialaised(size);
	if (res != nullptr) {
		return res;
	}

	if (size <= MAX_SIZE_TO_ALLOC_IN_SLAB) {
		char *res = alloc_block_in_slab(size);
		if (res == nullptr) { // debug
    		fatal_error("return nullptr in malloc !!!\n");
    	}
		return res;
	}
	if (size <= MAX_SIZE_TO_ALLOC_IN_CLUSTERS) {
		char *res = alloc_block_in_cluster(size);
		if (res == nullptr) { // debug
    		fatal_error("return nullptr in malloc !!!\n");
    	}
		return res;
	}
	res = alloc_big_block(size);
	if (res == nullptr) { // debug
		fatal_error("return nullptr in malloc !!!\n");
	}
	return res;
}

extern "C" void free(void *ptr) {
	if (is_constructors_begin_to_executing.load() == false) {
		// WRITE("free when is_constructors_begin_to_executing\n");
		free_big_block((char*)ptr);
    	return;
    }
	// lock_guard<recursive_mutex> lg(test_mutex);

	if (num_alive_writers > 0) {
		// print_to_console("free catched for print");
		if (ptr != nullptr) {
			free_big_block((char*)ptr);
		}
		return;
	}

	print("free catched \n");
	
    check_init();

    if (ptr == nullptr) {
    	return;
    }
    char* aligned_ptr = ((char*)ptr) - ((ull)ptr) % PAGE_SIZE;

    if (aligned_ptr == (char*)ptr) {
    	free_big_block((char*)ptr);
    	return;
    }
    int num_pages = get_num_of_pages_to_begin(aligned_ptr);

    if (num_pages < 0) {
    	free_block_in_clster((char*)ptr);
    } else {
    	free_block_in_slab((char*)ptr);
    }
}

extern "C" void *calloc(size_t nmemb, size_t size) {
	// lock_guard<recursive_mutex> lg(test_mutex);

	if (is_constructors_begin_to_executing.load() == false) {
		// WRITE("calloc when not is_constructors_begin_to_executing\n");
	} else {

		if (num_alive_writers > 0) {
			WRITE("in calloc for print\n");
			char* res = alloc_big_block(nmemb * size);
			if (res == nullptr) {
				WRITE("  Error in calloc for print : too small size of buffer\n");
				exit(1);
			}
			for (size_t w = 0; w < nmemb * size; w++) {
		    	res[w] = 0;
		    }
			return res;
		}
		print("in calloc : ", nmemb * size, "\n");
		check_init();
	}
    
	if ((nmemb == 0) || (size == 0) || (nmemb * size > MAX_SIZE_TO_ALLOC)) {
		fatal_error("nullptr in calloc\n");
		return nullptr;
	}
	if ((nmemb * size) / size != nmemb) {
		fatal_error("nullptr in calloc\n");
		return nullptr;
	}

	char* res = (char*)malloc(nmemb * size);
    if (res == nullptr) {
    	fatal_error("nullptr in calloc\n");
    	return nullptr;
    }

    // TODO заменить на memset или понаставить if-ов

    for (size_t w = 0; w < nmemb * size; w++) {
    	res[w] = 0;
    }
    return res;
}

extern "C" void *realloc(void *ptr, size_t size) {
	if (is_constructors_begin_to_executing.load() == false) {
		// WRITE("realloc when is_constructors_begin_to_executing\n");
		char* res = alloc_big_block(size);
		
		if (ptr == nullptr)  {
			return res;
		}
		size_t copy_size = min(size, malloc_usable_size_big_block((char*)ptr));
		
		for (size_t w = 0; w < copy_size; w++) {
			res[w] = ((char*)ptr)[w];
		}
		free_big_block((char*)ptr);
    	return res;
    }
	// lock_guard<recursive_mutex> lg(test_mutex);

	print("realloc catched: ", size, "\n");

    check_init();

	if (size == 0) {
    	free(ptr);
    	return nullptr;
    }
    if (ptr == nullptr) {
    	return malloc(size);
    }

    char* aligned_ptr = ((char*)ptr) - ((ull)ptr) % PAGE_SIZE;

    if (aligned_ptr == (char*)ptr) {
    	char *res = realloc_big_block((char*)ptr, size);
    	if (res == nullptr) { // debug
    		fatal_error("nullptr in realloc\n");
    	}
    	return res;
    }

    int num_pages = get_num_of_pages_to_begin(aligned_ptr);

    if (num_pages < 0) {
    	char *res = realloc_block_in_cluster((char*)ptr, size);
    	if (res == nullptr) { // debug
    		fatal_error("nullptr in realloc\n");
    	}
    	return res;
    } else {
    	return realloc_block_in_slab((char*)ptr, size);
    }
}

extern "C" void *reallocarray(void *ptr, size_t nmemb, size_t size) {
	// lock_guard<recursive_mutex> lg(test_mutex);
	if ((nmemb * size) / size != nmemb) {
		if (nullptr) { // debug
    		fatal_error("nullptr in reallocarray\n");
    	}
		return nullptr;
	}
	return realloc(ptr, nmemb * size);
}

extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size) {
	// lock_guard<recursive_mutex> lg(test_mutex);

	print("posix_memalign catched: ", alignment, " ", size, "\n");

    check_init();
    if (alignment < 8) {
    	alignment = 8;
    }
    if (alignment == 8) {
    	void* res = malloc(size);
    	if (res == nullptr) {
    		return ENOMEM;
    	}
    	*memptr = res;
    	return 0;
    }
	
	if ((alignment < 8) || ((alignment & (alignment - 1)) != 0)) {
    	return EINVAL;
    }
    if (size == 0) {
    	*memptr = nullptr;
    	return 0;
    }
    
    if (size % PAGE_SIZE != 0) {
    	size += PAGE_SIZE - size % PAGE_SIZE;
    }
	alignment = max(alignment, PAGE_SIZE);

	char *block = (char*)mmap(nullptr, size + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if (block == MAP_FAILED) {
		return ENOMEM;
	}
	ull left_shift = alignment - PAGE_SIZE - ((ull)block % alignment);
	ull right_shift = alignment - PAGE_SIZE - left_shift;

	if (left_shift > 0) {
		int error = munmap(block, left_shift);
		my_assert(error == 0, "error in munmup in posix_memalign (left_shift): ", errno);
	}
	if (right_shift > 0) {
		int error = munmap(block + size + alignment - right_shift, right_shift);
		my_assert(error == 0, "error in munmup in posix_memalign (right_shift): ", errno);
	}

	block += left_shift;

	char *res = block + PAGE_SIZE;

    *(char**)(res - 3 * sizeof(char*)) = block;
	*(size_t*)(res - 2 * sizeof(char*)) = size + PAGE_SIZE;

	*memptr = res;
	return 0;
}

extern "C" size_t malloc_usable_size(void *ptr) {
	// lock_guard<recursive_mutex> lg(test_mutex);
	check_init();

	print("malloc_usable_size catched : \n");

	if (ptr == nullptr) {
    	return 0;
    }
    char* aligned_ptr = ((char*)ptr) - ((ull)ptr) % PAGE_SIZE;

    if (aligned_ptr == (char*)ptr) {
    	return malloc_usable_size_big_block((char*)ptr);
    }
    int num_pages = get_num_of_pages_to_begin(aligned_ptr);

    print("get_num_of_pages_to_begin : ", num_pages, "\n");

    if (num_pages < 0) {
    	return malloc_usable_size_in_cluster((char*)ptr);
    }
    if (num_pages >= 0) {
    	return malloc_usable_size_in_slab((char*)ptr);
    }
}

////////////////////////////////////////////////////////////////////

extern "C" void *aligned_alloc(size_t alignment, size_t size) {
	fatal_error("aligned_alloc not implemented");
}
extern "C" void *valloc(size_t size) {
	fatal_error("valloc not implemented");
}
extern "C" void *memalign(size_t alignment, size_t size) {
	fatal_error("memalign not implemented");
}
extern "C" void *pvalloc(size_t size) {
	fatal_error("pvalloc not implemented");
}
extern "C" int mallopt(int param, int value) {
	fatal_error("mallopt not implemented");
}
extern "C" int malloc_trim(size_t pad) {
	fatal_error("malloc_trim not implemented");
}
extern "C" void* malloc_get_state(void) {
	fatal_error("malloc_get_state not implemented");
}
extern "C" int malloc_set_state(void *state) {
	fatal_error("malloc_set_state not implemented");
}
extern "C" int malloc_info(int options, FILE *stream) {
	fatal_error("malloc_info not implemented");
}
extern "C" void malloc_stats(void) {
	fatal_error("malloc_stats not implemented");
}
extern "C" void cfree(void *ptr) {
	fatal_error("cfree not implemented");
}

// LD_PRELOAD=./libAllocator.so subl
