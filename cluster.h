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
#include "storage_of_clusters.h"

class cluster {
public:
	static const int32_t MAX_RANG = CLUSTER_MAX_RANG;
	static const int32_t MIN_RANG = CLUSTER_MIN_RANG;
	static const int32_t SHIFT = 16;
	static const int32_t SERV_DATA_SIZE = CLUSTER_SERV_DATA_SIZE;
	static const int32_t RESERVED_RANG = 8;
private:
	typedef long long ll;

	void *first_block_of_data;

	char *storage;
	char* levels[MAX_RANG + 1];
	ll available_memory = 0;
public:
	/*
		Изменения prev_cluster и next_cluster происходят в рамках соответствующего storage_mutex-а, либо,
	когда cluster пустой, в рамках empty_sorage_mutex.
	*/
	cluster *prev_cluster, *next_cluster;

	std::recursive_mutex cluster_mutex;

	int max_available_rang;     // если max_available_rang != old_max_available_rang, то надо перевесить
	int old_max_available_rang; // cluster в его storadge_of_clusters

	storage_ptr this_storage_of_clusters;

	static int32_t get_rang(char *ptr);
	void set_rang(char *ptr, int32_t val);

	char* get_prev(char *ptr);
	void set_prev(char *ptr, char* val);

	char* get_next(char *ptr);
	char* set_next(char *ptr, char* val);
private:

	bool is_valid_ptr(char *ptr);// debug
	bool is_valid_rang(int rang);//debug

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

	void update_max_available_rang(); // пересчитывает max_available_rang, но сам ничего со своим расположением в storadge_of_clusters не делает

	void cut(char* block);
	void add_to_begin(int level, char* block);
	char* split(char* block, ll neded_level);

	cluster();
public:
	bool is_necessary_to_overbalance();
	bool is_empty();

	char *alloc(size_t size);
	void free(char* ptr);
	char *try_to_realloc(char *ptr, size_t new_rang_of_block);

	friend cluster* create_cluster();
};

static_assert((1<<cluster::RESERVED_RANG) >= sizeof(cluster), "too small RESERVED_RANG");
static_assert(cluster::RESERVED_RANG < cluster::MAX_RANG, "too big reserved rang");

static_assert(cluster::MAX_RANG <= RANG_OF_CLUSTERS, "invalid RANG_OF_CLUSTERS");

int32_t get_num_of_pages_to_begin(char *ptr);

cluster *create_cluster();
void destroy_cluster(cluster *c);

int calculate_optimal_rang(size_t size);

#endif // CLUSTER_H
