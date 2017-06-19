//#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

#include "cluster.h"
#include "debug.h"

// #define ORIGINAL

const int FIRST_RANG = 28;
cluster first;

test_struct test_1(123456);

static void* (*malloc_original)(size_t size) = nullptr;
static void (*free_original)(void *ptr) = nullptr;
static void* (*calloc_original)(size_t nmemb, size_t size) = nullptr;
static void* (*realloc_original)(void *ptr, size_t size) = nullptr;


extern "C" void *malloc(size_t size) {
	if (!first.is_initialized()) {
		first.init(FIRST_RANG);
	}
	// TODO
	#ifdef ORIGINAL
	    if (malloc_original == nullptr) {
	        malloc_original = (void* (*)(size_t size))dlsym(RTLD_NEXT, "malloc");
	        if (malloc_original == nullptr) {
	        	fatal_error("nullptr in dlsym(malloc)");
	            return nullptr;
	        }
	    }
	#endif
    // print("malloc catched: ", size, "\n");

    #ifndef ORIGINAL
    	return first.alloc(size);
    #else
    	return malloc_original(size);
    #endif
}

extern "C" void free(void *ptr) {
	// TODO
	#ifdef ORIGINAL
	    if (free_original == nullptr) {
	        free_original = (void (*)(void *ptr))dlsym(RTLD_NEXT, "free");
	        if (free_original == nullptr) {
	        	fatal_error("nullptr in dlsym(free)");
	            return;
	        }
	    }
    #endif

    // print("free catched\n");
   	
    #ifndef ORIGINAL
    	first.free((char*)ptr);
    #else
    	free_original(ptr);
    #endif
}

extern "C" void *calloc(size_t nmemb, size_t size) {
	// print("calloc catched\n");

	if (!first.is_initialized()) {
		first.init(FIRST_RANG);
	}

	// TODO
	#ifdef ORIGINAL
	    if (calloc_original == nullptr) {
	        calloc_original = (void* (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
	        if (calloc_original == nullptr) {
	        	fatal_error("nullptr in dlsym(calloc)");
	            return nullptr;
	        }
	    }
    #endif
   	
    #ifndef ORIGINAL
	    char* res = first.alloc(nmemb * size);
	    if (res == nullptr) {
	    	return nullptr;
	    }
	    for (size_t w = 0; w < nmemb * size; w++) {
	    	res[w] = 0;
	    }
	    return res;
    #else
    	return calloc_original(nmemb, size);
    #endif
}

extern "C" void *realloc(void *ptr, size_t size) {
	// print("realloc catched\n");

	if (!first.is_initialized()) {
		first.init(FIRST_RANG);
	}

	// TODO
	#ifdef ORIGINAL
	    if (realloc_original == nullptr) {
	        realloc_original = (void* (*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");
	        if (realloc_original == nullptr) {
	        	fatal_error("nullptr in dlsym(realloc)");
	            return nullptr;
	        }
	    }
    #endif
   	
    #ifndef ORIGINAL
	    if (size == 0) {
	    	first.free((char*)ptr);
	    	return nullptr;
	    }
	    if (ptr == nullptr) {
	    	return first.alloc(size);
	    }
	    return first.realloc((char*)ptr, size);
    #else
    	return realloc_original(ptr, size);
    #endif
}

// LD_PRELOAD=./libAllocator.so ./test.exe
