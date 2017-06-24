#ifndef CLUSTER_H
#define CLUSTER_H

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <sys/mman.h>
#include <cstring>

class cluster;

#include "constants.h"

#include "debug.h"
#include "storadge_of_clusters.h"

class cluster {
public:
	static const int32_t MAX_RANG = CLUSTER_MAX_RANG;
	static const int32_t MIN_RANG = CLUSTER_MIN_RANG;
	static const int32_t SHIFT = 16;
	static const int32_t SERV_DATA_SIZE = CLUSTER_SERV_DATA_SIZE;
	static const int32_t PAGE_SIZE = 4096;
	static const int32_t RESERVED_RANG = 12;
private:
	typedef long long ll;

	char *storage;
	char* levels[MAX_RANG + 1];
	ll available_memory = 0;
public:
	cluster *prev_cluster, *next_cluster;
	std::recursive_mutex cluster_mutex;

	int max_available_rang;     // если max_available_rang != old_max_available_rang, то надо перевесить
	int old_max_available_rang; // cluster в его storadge_of_clusters

	storadge_of_clusters *this_storadge_of_clusters;

	static int32_t get_rang(char *ptr);
	void set_rang(char *ptr, int32_t val);

	char* get_prev(char *ptr);
	void set_prev(char *ptr, char* val);

	char* get_next(char *ptr);
	char* set_next(char *ptr, char* val);
private:

	bool is_valid_ptr(char *ptr) {// debug
		return ((storage <= ptr) && (ptr < storage + (1 << RANG_OF_CLUSTERS)));
	}
	bool is_valid_rang(int rang) {//debug
		return ((MIN_RANG <= rang) && (rang <= MAX_RANG));
	}

	void print_state() {// debug
		for (int w = MAX_RANG; w >= MIN_RANG; w--) {
			print(w, " : ");
			if (levels[w] != nullptr) {
				print(get_prev(levels[w]) - storage, "\n");
				my_assert((get_prev(levels[w]) - storage) == -w, "incorrect level");
			}
			for (char* e = levels[w]; e != nullptr; e = get_next(e)) {
				my_assert((MIN_RANG <= get_rang(e)) && (get_rang(e) <= MAX_RANG), "incorrect rang");

				print(e - storage, " ", e - storage + (1<<w), " | ");
			}
			print("\n");
		}
		print("\n");
	}

	void update_max_available_rang() { // пересчитывает max_available_rang, но сам ничего со своим расположение в storadge_of_clusters не делает
		int w = MAX_RANG;
		while ((w >= MIN_RANG) && (levels[w] == nullptr)) {
			w--;
		}
		max_available_rang = w;
	}

	void cut(char* block);
	void add_to_begin(int level, char* block);
	char* split(char* block, ll neded_level);

	cluster();
public:

	char *alloc(size_t size);
	void free(char* ptr);
	char *try_to_realloc(char *ptr, size_t new_rang_of_block);

	friend cluster* create_cluster();
};

static_assert(cluster::PAGE_SIZE == (1<<cluster::RESERVED_RANG), "invalid RESERVED_RANG");
static_assert((1<<cluster::RESERVED_RANG) - cluster::SERV_DATA_SIZE >= sizeof(cluster), "too small RESERVED_RANG");
static_assert(cluster::RESERVED_RANG < cluster::MAX_RANG, "too big reserved rang");

static_assert(cluster::MAX_RANG <= RANG_OF_CLUSTERS, "invalid RANG_OF_CLUSTERS");

int32_t get_num_of_pages_to_begin(char *ptr);

cluster *create_cluster();

int calculate_optimal_rang(size_t size);

#endif // CLUSTER_H
