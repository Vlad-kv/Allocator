//#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <mutex>
#include <atomic>

#include "debug.h"

#include "work_with_big_blocks.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

// #define ORIGINAL

#define WRITE(str) write(1, str, strlen(str));

typedef unsigned long long ull;

storadge_of_clusters first_stor;

std::recursive_mutex test_mutex;

std::atomic_bool is_initialized;
std::mutex init_mutex;

void initialize() {
	first_stor.init();
	first_stor.add_cluster(create_cluster());
}

void check_init() {
	if (is_initialized.load() == true) {
		return;
	}
	std::lock_guard<std::mutex> lg(init_mutex);

	if (is_initialized.load() == true) {
		return;
	}
	is_initialized = true;
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
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

	if (num_alive_writers > 0) {
		// WRITE("  malloc for print\n");
		char* res = buffer_for_print + buffer_pos;
		buffer_pos += size;
		if (buffer_pos > BUFFER_FOR_PRINT_SIZE) {
			WRITE("  Error in malloc for print : too small size of buffer\n");
			exit(1);
		}
		return res;
	}

	if (size == (size_t)-1) {               // debug
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
	    check_init();

		if ((size == 0) || (size > MAX_SIZE_TO_ALLOC)) {
			return nullptr;
		}

		if (size <= MAX_SIZE_TO_ALLOC_IN_SLAB) {
			return alloc_block_in_slab(size);
		}
		if (size <= MAX_SIZE_TO_ALLOC_IN_CLUSTERS) {
			return alloc_block_in_cluster(size);
		}
		return alloc_big_block(size);
	#endif
}

extern "C" void free(void *ptr) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

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

	    print("    after check_init\n");
	    char* aligned_ptr = ((char*)ptr) - ((ull)ptr) % cluster::PAGE_SIZE;
	    int num_pages = get_num_of_pages_to_begin(aligned_ptr);

	    print("    after get_num_of_pages_to_begin\n");

	    if (num_pages < 0) {
	    	free_block_in_clster((char*)ptr);
	    }
	    if (num_pages == 0) {
	    	free_big_block((char*)ptr);
	    }
	    if (num_pages > 0) {
	    	free_block_in_slab((char*)ptr);
	    }
	    print("    after free\n");
    #endif
}

extern "C" void *calloc(size_t nmemb, size_t size) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

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
	    print("    after calloc\n");
	    return res;
    #endif
}

extern "C" void *realloc(void *ptr, size_t size) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

	print("realloc catched\n");

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

	    char* aligned_ptr = ((char*)ptr) - ((ull)ptr) % cluster::PAGE_SIZE;
	    int num_pages = get_num_of_pages_to_begin(aligned_ptr);

	    if (num_pages < 0) {
	    	return realloc_block_in_cluster((char*)ptr, size);
	    }
	    if (num_pages == 0) {
	    	return realloc_big_block((char*)ptr, size);
	    }
	    if (num_pages > 0) {
	    	return realloc_block_in_slab((char*)ptr, size);
	    }
    #endif
}

extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

	print("posix_memalign catched\n");

	#ifdef ORIGINAL
	    if (posix_memalign_original == nullptr) {
	        posix_memalign_original = (int (*)(void **memptr, size_t alignment, size_t size) )dlsym(RTLD_NEXT, "posix_memalign");
	        if (posix_memalign_original == nullptr) {
	        	fatal_error("nullptr in dlsym(realloc)");
	            return ENOMEM;
	        }
	    }
	    return posix_memalign(memptr, alignment, size);
	#else
	    check_init();
	    // TODO сделать
		fatal_error("Not implemented.");
    #endif
}

// LD_PRELOAD=./libAllocator.so ./test.exe
