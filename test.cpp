#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include <fstream>

using namespace std;

static void* (*malloc_original)(size_t size) = nullptr;
static void (*free_original)(void *ptr) = nullptr;
static void* (*calloc_original)(size_t nmemb, size_t size) = nullptr;
static void* (*realloc_original)(void *ptr, size_t size) = nullptr;


const int MAX_SUMM_MEMORY = 1000000;
const int MAX_SIZE = 2000;
const int NUM_TO_FREE_IN_BIG_FREE = 100;

const int MIN_SIZE_TO_ALLOC = 32;
const int MAX_SIZE_TO_ALLOC = 2000;

static_assert(MIN_SIZE_TO_ALLOC <= MAX_SIZE_TO_ALLOC, "MIN_SIZE_TO_ALLOC must be <= MAX_SIZE_TO_ALLOC");

size_t get_size() {
	return MIN_SIZE_TO_ALLOC + rand() % (MAX_SIZE_TO_ALLOC - MIN_SIZE_TO_ALLOC + 1);
}



int summ_memory = 0;

char *a_orig[MAX_SIZE];
char *a_my[MAX_SIZE];
int alloc_size[MAX_SIZE];
int a_size = 0;

void make_free(size_t pos) {
	free_original(a_orig[pos]);
	a_orig[pos] = a_orig[a_size - 1];

	free(a_my[pos]);
	a_my[pos] = a_my[a_size - 1];

	summ_memory -= alloc_size[pos];
	alloc_size[pos] = alloc_size[a_size - 1];
	a_size--;
}

void big_free() {
	for (int w = 0; w < NUM_TO_FREE_IN_BIG_FREE; w++) {
		if (a_size == 0) {
			break;
		}
		make_free(rand() % a_size);
	}
}

void make_malloc(size_t size) {
	if (a_size == MAX_SIZE) {
		big_free();
	}
	while (size > MAX_SUMM_MEMORY - summ_memory) {
		big_free();
	}
	a_orig[a_size] = (char*)malloc_original(size);
	a_my[a_size] = (char*)malloc(size);

	if (a_my[a_size] == nullptr) {
		cout << "nullptr in malloc";
		exit(1);
	}
	for (int w = 0; w < size; w++) {
		a_orig[a_size][w] = a_my[a_size][w] = (char)rand();
	}
	alloc_size[a_size] = size;
	a_size++;
	summ_memory += size;
}

void make_calloc(size_t size) {
	if (a_size == MAX_SIZE) {
		big_free();
	}
	while (size > MAX_SUMM_MEMORY - summ_memory) {
		big_free();
	}
	a_orig[a_size] = (char*)calloc_original(size, 1);
	a_my[a_size] = (char*)calloc(size, 1);

	if (a_my[a_size] == nullptr) {
		cout << "nullptr in calloc";
		exit(1);
	}
	alloc_size[a_size] = size;
	a_size++;
	summ_memory += size;
}

void make_realloc() {
	if (a_size > 0) {
		int pos = rand() % a_size;
		int new_size = get_size();

		char* l_a_orig = a_orig[pos];
		char* l_a_my = a_my[pos];
		int old_size = alloc_size[pos];

		a_orig[pos] = a_orig[a_size - 1];
		a_my[pos] = a_my[a_size - 1];
		alloc_size[pos] = alloc_size[a_size - 1];
		a_size--;

		while (summ_memory + (new_size - old_size) > MAX_SUMM_MEMORY) {
			big_free();
		}

		l_a_orig = (char*)realloc_original(l_a_orig, new_size);
		l_a_my = (char*)realloc(l_a_my, new_size);

		if (l_a_my == nullptr) {
			cout << "nullptr in realloc";
			exit(1);
		}
		for (int w = old_size; w < new_size; w++) {
			l_a_orig[w] = l_a_my[w] = (char)rand();
		}

		a_orig[a_size] = l_a_orig;
		a_my[a_size] = l_a_my;
		alloc_size[a_size] = new_size;
		a_size++;

		summ_memory += new_size - old_size;
	}
}

void big_check() {
	for (int w = 0; w < a_size; w++) {
		for (int e = 0; e < alloc_size[w]; e++) {
			if (a_orig[w][e] != a_my[w][e]) {
				cout << (long long)(a_orig[w] + e) << " " << (long long)(a_my[w] + e) << "\n";
				cout << "big_check failed on " << w << " " << e << " expected " <<
					a_orig[w][e] << " found " << a_my[w][e] << "     " << alloc_size[w] << " \n";
				exit(1);
			}
			a_orig[w][e] = a_my[w][e] = (char)rand();
		}
	}
}

const int STEP = 200;

void test(int num_mallocs, int num_reallocs, int num_callocs, int num_free, int num_modifications) {
	num_reallocs += num_mallocs;
	num_callocs += num_reallocs;
	num_free += num_callocs;
	num_modifications += num_free;

	for (int w = 0; w < 3; w++) {
		if ((w != 0) && (w % STEP == 0)) {
			cout << "Performed " << w << " operations ";
			big_check();
			cout << "big_check perfomed\n";
		}

		int c = rand() % num_free;

		if (c < num_mallocs) {
			make_malloc(get_size());
			continue;
		}
		if (c < num_reallocs) {
			make_realloc();
			continue;
		}
		if (c < num_callocs) {
			make_calloc(get_size());
			continue;
		}
		if (c < num_free) {
			if (a_size > 0) {
				make_free(rand() % a_size);
			}
			continue;
		}
		if (c < num_modifications) {
			int no = rand() % a_size;
			int pos = rand() % alloc_size[no];

			a_orig[no][pos] = a_my[no][pos] = (char)rand();
			continue;
		}
	}
}

int main(int argc, char *argv[]) {
	malloc_original = (void* (*)(size_t size))dlsym(RTLD_NEXT, "malloc");
	free_original = (void (*)(void *ptr))dlsym(RTLD_NEXT, "free");
	calloc_original = (void* (*)(size_t, size_t))dlsym(RTLD_NEXT, "calloc");
	realloc_original = (void* (*)(void *, size_t))dlsym(RTLD_NEXT, "realloc");

	test(3, 4, 3, 2, 10);

	system("echo 1");

	/*
	ofstream out("log.txt", ios_base::app);
	out << "321\n";
	out.close();
	*/
	return 0;
}
