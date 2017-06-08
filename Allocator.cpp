//#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

#include "cluster.h"
#include "debug.h"

//#define ORIGINAL

cluster first;

test_struct test_1(123456);

static void* (*malloc_original)(size_t size) = NULL;

extern "C" void *malloc(size_t size) {
	if (!first.is_initialized()) {
		first.init(22);
	}
	// TODO
	#ifdef ORIGINAL
	    if (malloc_original == NULL) {
	        malloc_original = (void* (*)(size_t size))dlsym(RTLD_NEXT, "malloc");
	        if (malloc_original == NULL) {
	            return NULL;
	        }
	    }
	#endif
    print("malloc catched: ", size, "\n");

    //print(test_1.val, "  !?!?!?!?!?!?!?!??\n\n");

    #ifndef ORIGINAL
    	return first.alloc(size);
    #else
    	return malloc_original(size);
    #endif
}

static void (*free_original)(void *ptr) = NULL;

extern "C" void free(void *ptr) {
	// TODO
	#ifdef ORIGINAL
	    if (free_original == NULL) {
	        free_original = (void (*)(void *ptr))dlsym(RTLD_NEXT, "free");
	        if (free_original == NULL) {
	        	write(1, "Error!\n", 7);
	            return;
	        }
	    }
    #endif

    print("free catched\n");
   	
    #ifndef ORIGINAL
    	first.free((char*)ptr);
    #else
    	free_original(ptr);
    #endif
}

// LD_PRELOAD=./libAllocator.so ./test.exe
