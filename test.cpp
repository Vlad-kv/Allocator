#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <cassert>

#include <vector>

#include <malloc.h>
#include <atomic>
#include <mutex>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <regex.h>
#include <sys/mman.h>

#include <fstream>
#include <chrono>
#include <thread>
#include <condition_variable>

using namespace std;

typedef unsigned long long ull;

static atomic<void* (*)(size_t size)> malloc_original;
static atomic<void (*)(void *ptr)> free_original;
static atomic<void* (*)(size_t nmemb, size_t size)> calloc_original;
static atomic<void* (*)(void *ptr, size_t size)> realloc_original;
static atomic<int (*)(void **memptr, size_t alignment, size_t size)> posix_memalign_original;

const int MAX_SUMM_MEMORY = 70000;
const int MAX_SIZE = 4000;
const int NUM_TO_FREE_IN_BIG_FREE = 5;

const int MIN_SIZE_TO_ALLOC = 10;
const int MAX_SIZE_TO_ALLOC = 5000;

const int MIN_MEMALIGN = 3, MAX_MEMALIGN = 15;

static_assert(MIN_SIZE_TO_ALLOC <= MAX_SIZE_TO_ALLOC, "MIN_SIZE_TO_ALLOC must be <= MAX_SIZE_TO_ALLOC");

size_t get_size() {
	return MIN_SIZE_TO_ALLOC + rand() % (MAX_SIZE_TO_ALLOC - MIN_SIZE_TO_ALLOC + 1);
}

mutex write_mutex;

thread_local int summ_memory = 0;

thread_local char *a_orig[MAX_SIZE];
thread_local char *a_my[MAX_SIZE];
thread_local int alloc_size[MAX_SIZE];
thread_local int a_size = 0;

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

void make_posix_memalign(size_t alignment, size_t size) {
	if (a_size == MAX_SIZE) {
		big_free();
	}
	while (size > MAX_SUMM_MEMORY - summ_memory) {
		big_free();
	}

	int res_1 = posix_memalign_original((void**)&a_orig[a_size], alignment, size);
	int res_2 = posix_memalign((void**)&a_my[a_size], alignment, size);

	if (res_2 != 0) {
		cout << "error in make_posix_memalign\n";
		exit(1);
	}

	assert((ull)a_my[a_size] % alignment == 0);

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
		int calc_size = malloc_usable_size(a_my[w]);
		if (calc_size < alloc_size[w]) {
			cout << "error in big_check : expected size ";
			cout << alloc_size[w] << ", found " << calc_size << "\n";
			exit(1);
		}
		for (int e = 0; e < alloc_size[w]; e++) {
			if (a_orig[w][e] != a_my[w][e]) {
				cout << (long long)(a_orig[w] + e) << " " << (long long)(a_my[w] + e) << "\n";
				cout << "big_check failed on " << w << " " << e << " expected " <<
					(int)a_orig[w][e] << " found " << (int)a_my[w][e] << "     " << alloc_size[w] << " \n";
				exit(1);
			}
			a_orig[w][e] = a_my[w][e] = (char)rand();
		}
	}
}

const int STEP = 10;

void test(int num_mallocs, int num_reallocs, int num_callocs, int num_free,
		int num_modifications, int num_posix_memaligns) {
	num_reallocs += num_mallocs;
	num_callocs += num_reallocs;
	num_free += num_callocs;
	num_modifications += num_free;
	num_posix_memaligns += num_modifications;

	for (int w = 0; w < 20000; w++) {
		if ((w != 0) && (w % STEP == 0)) {
			{
				lock_guard<mutex> lg(write_mutex);
				cout << "Performed " << w << " operations ";
			}
			big_check();
			{
				lock_guard<mutex> lg(write_mutex);
				cout << "big_check perfomed\n";
			}
		}

		int c = rand() % num_posix_memaligns;

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
			if (a_size > 0) {
				int no = rand() % a_size;
				int pos = rand() % alloc_size[no];

				a_orig[no][pos] = a_my[no][pos] = (char)rand();
			}
			continue;
		}
		if (c < num_posix_memaligns) {
			int c = rand() % (MAX_MEMALIGN - MIN_MEMALIGN + 1) + MIN_MEMALIGN;
			int alignment = 1;
			while (c > 0) {
				c--;
				alignment *= 2;
			}
			make_posix_memalign(alignment, get_size());
			continue;
		}
	}
	while (a_size > 0) {
		big_free();
	}
}


atomic_flag flag = ATOMIC_FLAG_INIT;
char *test_ptr;
bool is_initialaised = false;

mutex test_mutex;
condition_variable cv;

void test_f() {
	if (flag.test_and_set() == false) {
		char *ptr = (char*)malloc(1000);
		{
			unique_lock<mutex> ul(test_mutex);
			is_initialaised = true;
			test_ptr = ptr;
			cv.notify_one();
		}
		this_thread::sleep_for(chrono::milliseconds(1000));
	} else {
		unique_lock<mutex> ul(test_mutex);
		while (is_initialaised == false) {
			cv.wait(ul);
		}
		free(test_ptr);
		this_thread::sleep_for(chrono::milliseconds(2000));
	}
}

string get_error(int error) {
	switch (error) {
		case E2BIG: return "E2BIG";

		case EACCES: return "EACCES";
		case EAGAIN: return "EAGAIN";
		case EFAULT: return "EFAULT";
		case EINVAL: return "EINVAL";

		case EIO: return "EIO";
		case EISDIR: return "EISDIR";
		case ELIBBAD: return "ELIBBAD";
		case ELOOP: return "ELOOP";

		case EMFILE: return "EMFILE";
		case ENAMETOOLONG: return "ENAMETOOLONG";
		case ENFILE: return "ENFILE";
		case ENOENT: return "ENOENT";
		case ENOEXEC: return "ENOEXEC";
		case ENOMEM: return "ENOMEM";
		case ENOTDIR: return "ENOTDIR";
		case EPERM: return "EPERM";
	}
	return "uncnown error";
}

int main(int argc, char *argv[]) {
	// malloc_original = (void* (*)(size_t size))malloc(-1);
	// free_original = (void (*)(void *ptr))malloc(-2);
	// calloc_original = (void* (*)(size_t, size_t))malloc(-3);
	// realloc_original = (void* (*)(void *, size_t))malloc(-4);
	// posix_memalign_original = (int (*)(void **memptr, size_t alignment, size_t size))malloc(-5);

	// srand(0); // 0 (5)

	// test(30, 40, 30, 60, 20, 50);

	// thread t_1(test_f), t_2(test_f);

	// t_1.join();
	// t_2.join();

	// cout << "!!!\n";

	cout << "in text.cpp\n";

	int error = execvp("./test_2.exe", argv);

	cout << "error : " << error << " " << get_error(errno) << "\n";

	cout << "after execvp\n";

	return 0;
}
