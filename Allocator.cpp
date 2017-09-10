//#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <mutex>
#include <atomic>
#include <sys/mman.h>

#include "debug.h"

#include "work_with_big_blocks.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

// #define ORIGINAL

#define WRITE(str) write(1, str, strlen(str));

typedef unsigned long long ull;
using namespace std;

recursive_mutex test_mutex;

extern std::atomic_int num_alive_writers;

atomic_bool is_initialized;
atomic_bool is_constructors_begin_to_executing;
mutex init_mutex;

struct test_struct {
	test_struct() {
		atomic_init<int>(&num_alive_writers, 0);

		is_constructors_begin_to_executing.store(true);
		is_initialized.store(false);

		print("in test_struct\n");
	}
	~test_struct() {
		print("in ~test_struct\n");
	}
	int w = 1;
};
test_struct test;

void initialize() {
	print("in initialize\n");

	init_global_clusters_data();
	init_slab_allocation();

	print("after initialize\n");
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

#ifdef ORIGINAL
	void* (*malloc_original)(size_t size) = nullptr;
	void (*free_original)(void *ptr) = nullptr;
	void* (*calloc_original)(size_t nmemb, size_t size) = nullptr;
	void* (*realloc_original)(void *ptr, size_t size) = nullptr;
	int (*posix_memalign_original)(void **memptr, size_t alignment, size_t size) = nullptr;
	bool going_to_init_original_calloc = 0;

	const int BUFF_SIZE = 100;
	char buffer[BUFF_SIZE];
	int next = 0;
#endif

extern "C" void *malloc(size_t size) {
	lock_guard<recursive_mutex> lg(test_mutex);

	if (num_alive_writers > 0) {
		// WRITE("  malloc for print\n");
		while ((ull)(buffer_for_print + buffer_pos) % 8 != 0) {
			buffer_pos++;
		}
		char* res = buffer_for_print + buffer_pos;
		buffer_pos += size;
		if (buffer_pos > BUFFER_FOR_PRINT_SIZE) {
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

	#ifdef ORIGINAL
	    if (malloc_original == nullptr) {
	        malloc_original = (void* (*)(size_t size))dlsym(RTLD_NEXT, "malloc");
	        if (malloc_original == nullptr) {
	        	fatal_error("nullptr in dlsym(malloc)");
	            return nullptr;
	        }
	    }
	    return malloc_original(size);
	#else
	    if (is_constructors_begin_to_executing.load() == false) {
	    	char *res = alloc_big_block(size);
	    	return res;
	    }
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
			return res;
		}
		if (size <= MAX_SIZE_TO_ALLOC_IN_CLUSTERS) {
			char *res = alloc_block_in_cluster(size);
			return res;
		}
		res = alloc_big_block(size);
		return res;
	#endif
}

extern "C" void free(void *ptr) {
	lock_guard<recursive_mutex> lg(test_mutex);

	if (num_alive_writers > 0) {
		// WRITE("  free for print\n");
		return;
	}

	print("free catched \n");
	
	#ifdef ORIGINAL
	    if (free_original == nullptr) {
	        free_original = (void (*)(void *ptr))dlsym(RTLD_NEXT, "free");
	        if (free_original == nullptr) {
	        	fatal_error("nullptr in dlsym(free)");
	            return;
	        }
	    }
	    if (!((buffer <= ptr) && (ptr < buffer + BUFF_SIZE))) {
	    	free_original(ptr);
	    }
	#else
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
    #endif
}

extern "C" void *calloc(size_t nmemb, size_t size) {
	lock_guard<recursive_mutex> lg(test_mutex);

	print("calloc catched : ", nmemb, " ", size, "\n");

	#ifdef ORIGINAL
	    if (calloc_original == nullptr) {
	    	if (!going_to_init_original_calloc) {     // всё из-за того, что dlsym сам вызывает calloc
	    		going_to_init_original_calloc = true;
	    	} else {
	    		char *res = buffer + next;
	    		next += nmemb * size;
	    		my_assert(next <= BUFF_SIZE, "too small buffer (in calloc)");

	    		for (int w = 0; w < nmemb * size; w++) {
	    			res[w] = 0;
	    		}
	    		return res;
	    	}

	        calloc_original = (void* (*)(size_t nmemb, size_t size))dlsym(RTLD_NEXT, "calloc");

	        if (calloc_original == nullptr) {
	        	fatal_error("nullptr in dlsym(calloc)");
	            return nullptr;
	        }
	    }
	    return calloc_original(nmemb, size);
	#else
	    check_init();
	    
		if ((nmemb == 0) || (size == 0) || (nmemb * size > MAX_SIZE_TO_ALLOC)) {
			return nullptr;
		}
		if ((nmemb * size) / size != nmemb) {
			return nullptr;
		}

		char* res = (char*)malloc(nmemb * size);
	    if (res == nullptr) {
	    	return nullptr;
	    }

	    // TODO заменить на memset или понаставить if-ов

	    for (size_t w = 0; w < nmemb * size; w++) {
	    	res[w] = 0;
	    }
	    return res;
    #endif
}

extern "C" void *realloc(void *ptr, size_t size) {
	lock_guard<recursive_mutex> lg(test_mutex);

	print("realloc catched: ", size, "\n");

	#ifdef ORIGINAL
	    if (realloc_original == nullptr) {
	        realloc_original = (void* (*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
	        if (realloc_original == nullptr) {
	        	fatal_error("nullptr in dlsym(realloc)");
	            return nullptr;
	        }
	    }
	    return realloc_original(ptr, size);
	#else
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
	    	return realloc_big_block((char*)ptr, size);
	    }

	    int num_pages = get_num_of_pages_to_begin(aligned_ptr);

	    if (num_pages < 0) {
	    	return realloc_block_in_cluster((char*)ptr, size);
	    } else {
	    	return realloc_block_in_slab((char*)ptr, size);
	    }
    #endif
}

extern "C" void *reallocarray(void *ptr, size_t nmemb, size_t size) {
	lock_guard<recursive_mutex> lg(test_mutex);
	if ((nmemb * size) / size != nmemb) {
		return nullptr;
	}
	return realloc(ptr, nmemb * size);
}

extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size) {
	lock_guard<recursive_mutex> lg(test_mutex);

	print("posix_memalign catched: ", alignment, " ", size, "\n");

	#ifdef ORIGINAL
	    if (posix_memalign_original == nullptr) {
	        posix_memalign_original = (int (*)(void **memptr, size_t alignment, size_t size) )dlsym(RTLD_NEXT, "posix_memalign");
	        if (posix_memalign_original == nullptr) {
	        	fatal_error("nullptr in dlsym(realloc)");
	            return ENOMEM;
	        }
	    }
	    return posix_memalign_original(memptr, alignment, size);
	#else
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
    #endif
}

extern "C" size_t malloc_usable_size(void *ptr) {
	lock_guard<recursive_mutex> lg(test_mutex);
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

    if (num_pages < 0) {
    	return malloc_usable_size_in_cluster((char*)ptr);
    }
    if (num_pages >= 0) {
    	return malloc_usable_size_in_slab((char*)ptr);
    }
}

///////////////////////////////////////////////////////////////////

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
	fatal_error("malloc_info not implemented");
}

// LD_PRELOAD=./libAllocator.so subl
