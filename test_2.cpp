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
#include <cassert>

using namespace std;

typedef unsigned long long ull;

static void* (*malloc_original)(size_t size) = nullptr;
static void (*free_original)(void *ptr) = nullptr;
static void* (*calloc_original)(size_t nmemb, size_t size) = nullptr;
static void* (*realloc_original)(void *ptr, size_t size) = nullptr;
static int (*posix_memalign_original)(void **memptr, size_t alignment, size_t size) = nullptr;

const int MAX_SIZE = 1000000;

char *a_orig[MAX_SIZE];
char *a_my[MAX_SIZE];
int alloc_size[MAX_SIZE];

int max_used = 0;


void print_error_res() {
	ofstream out("test_2_res.txt");

	out.close();
	exit(0);
}

void make_free(size_t pos) {
	if (pos == 0) {
		return;
	}
	if (a_orig[pos] == nullptr) {
		cout << "Error! - attempt to free nullptr\n";
		exit(1);
	}

	free_original(a_orig[pos]);
	a_orig[pos] = nullptr;

	free(a_my[pos]);
	a_my[pos] = nullptr;

	alloc_size[pos] = 0;
}

void make_malloc(size_t size, int pos) {
	if (pos == MAX_SIZE) {
		cout << "Error - too small MAX_SIZE";
		exit(1);
	}
	a_orig[pos] = (char*)malloc_original(size);
	a_my[pos] = (char*)malloc(size);

	if (a_my[pos] == nullptr) {
		cout << "nullptr in malloc";
		exit(1);
	}
	for (int w = 0; w < size; w++) {
		a_orig[pos][w] = a_my[pos][w] = (char)rand();
	}
	alloc_size[pos] = size;
	max_used = max(max_used, pos);
}

void make_posix_memalign(size_t alignment, size_t size, int pos) {
	if (pos == MAX_SIZE) {
		cout << "Error - too small MAX_SIZE";
		exit(1);
	}

	int res_1 = posix_memalign_original((void**)&a_orig[pos], alignment, size);
	int res_2 = posix_memalign((void**)&a_my[pos], alignment, size);

	if (res_2 != 0) {
		cout << "error in make_posix_memalign\n";
		exit(1);
	}

	assert((ull)a_my[pos] % alignment == 0);

	for (int w = 0; w < size; w++) {
		a_orig[pos][w] = a_my[pos][w] = (char)rand();
	}
	alloc_size[pos] = size;
	max_used = max(max_used, pos);
}

void make_calloc(size_t size, int pos) {
	if (pos == MAX_SIZE) {
		cout << "Error - too small MAX_SIZE";
		exit(1);
	}
	a_orig[pos] = (char*)calloc_original(size, 1);
	a_my[pos] = (char*)calloc(size, 1);

	if (a_my[pos] == nullptr) {
		cout << "nullptr in calloc";
		exit(1);
	}
	alloc_size[pos] = size;
	max_used = max(max_used, pos);
}

void make_realloc(int old_pos, int size, int new_pos) {
	if (new_pos == MAX_SIZE) {
		cout << "Error - too small MAX_SIZE";
		exit(1);
	}
	a_orig[new_pos] = (char*)realloc_original(a_orig[old_pos], size);
	a_my[new_pos] = (char*)realloc(a_my[old_pos], size);
	alloc_size[new_pos] = size;

	for (int w = alloc_size[old_pos]; w < size; w++) {
		a_my[new_pos][w] = a_orig[new_pos][w] = (char)rand();
	}

	a_orig[old_pos] = nullptr;
	a_my[old_pos] = nullptr;
	alloc_size[old_pos] = 0;

	if (a_my[new_pos] == nullptr) {
		cout << "nullptr in realloc";
		exit(1);
	}

	max_used = max(max_used, new_pos);
}

const int MAX_NUM_OF_ERRORS = 100;



void big_check() {
	for (int w = 0; w <= max_used; w++) {
		int error = 0;
		for (int e = 0; e < alloc_size[w]; e++) {
			if (a_orig[w][e] != a_my[w][e]) {
				cout << (ull)(a_orig[w] + e) << " " << (ull)(a_my[w] + e) << "\n";
				cout << "fail :" << w << " " << e << " expected " <<
					(int)a_orig[w][e] << " found " << (int)a_my[w][e] << "     " << alloc_size[w] << " \n";
				// exit(1);
				error++;
				if (error > MAX_NUM_OF_ERRORS) {
					exit(1);
				}
			}
			if (!error) {
				a_orig[w][e] = a_my[w][e] = (char)rand();
			}
		}
		if (error) {
			exit(1);
		}
	}
}

const int STEP = 1;

int main(int argc, char *argv[]) {
	malloc_original = (void* (*)(size_t size))malloc(-1);
	free_original = (void (*)(void *ptr))malloc(-2);
	calloc_original = (void* (*)(size_t, size_t))malloc(-3);
	realloc_original = (void* (*)(void *, size_t))malloc(-4);
	posix_memalign_original = (int (*)(void **memptr, size_t alignment, size_t size))malloc(-5);


	freopen("test_2_input.txt", "r", stdin);
	int num;
	cin >> num;

	alloc_size[0] = 0;

	char buff[101];
	for (int w = 0; w < num; w++) {
		cin >> buff;
		if (buff[0] == 'm') {
			int size, pos;
			cin >> size >> pos;
			make_malloc(size, pos);
		}
		if (buff[0] == 'p') {
			int alignment, size, pos;
			cin >> alignment >> size >> pos;
			make_posix_memalign(alignment, size, pos);
		}
		if (buff[0] == 'c') {
			int nmemb, size, pos;
			cin >> nmemb >> size >> pos;
			if ((min(nmemb, size) == 0) || ((nmemb * size) / size == nmemb)) {
				make_calloc(nmemb * size, pos);
			}
		}
		if (buff[0] == 'f') {
			int pos;
			cin >> pos;
			make_free(pos);
		}
		if (buff[0] == 'r') {
			int old_pos, size, new_pos;
			cin >> old_pos >> size >> new_pos;
			make_realloc(old_pos, size, new_pos);
		}
		if (buff[0] == '#') {
			return 0;
		}

		if (w % STEP == 0) {
			cout << w + 1 << " ";
			big_check();
			cout << "#\n";
		}
	}
	return 0;
}
