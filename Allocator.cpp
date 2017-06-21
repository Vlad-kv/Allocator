//#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>
#include <mutex>

#include "cluster.h"
#include "debug.h"

// #define ORIGINAL

#define WRITE(str) write(1, str, strlen(str));

const int FIRST_RANG = 27;
cluster first;

test_struct test;

std::recursive_mutex test_mutex;

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

	if (test.is_initialaised == 0) {
		print(" ### ");
	}

	print("malloc catched: ", size, "\n");

	// TODO
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
	    if (!first.is_initialized()) {
			first.init(FIRST_RANG);
		}
		return first.alloc(size);
	#endif
}

extern "C" void free(void *ptr) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

	if (num_alive_writers > 0) {
		// WRITE("  free for print\n");
		return;
	}

	print("free catched : ", (long long)ptr, "\n");
	// TODO
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

	    first.free((char*)ptr);
    #endif
}


extern "C" void *calloc(size_t nmemb, size_t size) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

	print("calloc catched : ", nmemb, " ", size, "\n");

	// TODO
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
		// return nullptr;
	#else
	    if (!first.is_initialized()) {
			first.init(FIRST_RANG);
		}

		char* res = first.alloc(nmemb * size);
	    if (res == nullptr) {
	    	return nullptr;
	    }
	    for (size_t w = 0; w < nmemb * size; w++) {
	    	res[w] = 0;
	    }
	    return res;
    #endif
}

extern "C" void *realloc(void *ptr, size_t size) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

	print("realloc catched\n");

	// TODO
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
	    if (!first.is_initialized()) {
			first.init(FIRST_RANG);
		}

		if (size == 0) {
	    	first.free((char*)ptr);
	    	return nullptr;
	    }
	    if (ptr == nullptr) {
	    	return first.alloc(size);
	    }
	    return first.realloc((char*)ptr, size);
    #endif
}

extern "C" int posix_memalign(void **memptr, size_t alignment, size_t size) {
	std::lock_guard<std::recursive_mutex> lg(test_mutex);

	if (test.is_initialaised == 0) {
		print(" ### ");
	}

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
	    if (!first.is_initialized()) {
			first.init(FIRST_RANG);
		}
		fatal_error("Not implemented.");
    #endif
}

// LD_PRELOAD=./libAllocator.so ./test.exe
