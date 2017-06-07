//#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <unistd.h>

#include "cluster.h"

static void* (*malloc_original)(size_t size) = NULL;

extern "C" void *malloc(size_t size) {
	// TODO
    if (malloc_original == NULL) {
        malloc_original = (void* (*)(size_t size))dlsym(RTLD_NEXT, "malloc");
        if (malloc_original == NULL) {
            return NULL;
        }
    }
    write(1, "malloc catched\n", 15);
    return malloc_original(size);
}

static void (*free_original)(void *ptr) = NULL;

extern "C" void free(void *ptr) {
	// TODO
    if (free_original == NULL) {
        free_original = (void (*)(void *ptr))dlsym(RTLD_NEXT, "free");
        if (free_original == NULL) {
        	write(1, "Error!\n", 7);
            return;
        }
    }
    write(1, "free catched\n", 15);
    free_original(ptr);
}


