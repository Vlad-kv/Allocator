#include "work_with_big_blocks.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

#include "cluster.h"
#include "storadge_of_clusters.h"
#include "constants.h"
#include <mutex>
#include <cstring>

typedef unsigned long long ull;

extern storadge_of_clusters first_stor;

cluster *get_pseudo_free_cluster(int rang) {
	// TODO
	return create_cluster();
}

char *add_pseudo_free_cluster_and_get_block(storadge_of_clusters *storadge, int rang) {
	print("In add_pseudo_free_cluster_and_get_block\n");

	cluster *new_cluster = get_pseudo_free_cluster(rang);

	std::lock_guard<std::recursive_mutex> lg(new_cluster->cluster_mutex);

	char *res = new_cluster->alloc(rang);
	storadge->add_cluster(new_cluster);

	return res;
}

cluster *get_begin_of_cluster(char *ptr) {
	char* aligned_ptr = (char*)(ptr - ((ull)ptr) % cluster::PAGE_SIZE);
	int num_pages = get_num_of_pages_to_begin(aligned_ptr);

	my_assert(num_pages < 0, "invalid num_pages");
	return (cluster*)(aligned_ptr + num_pages * (ull)cluster::PAGE_SIZE);
}

char *alloc_block_in_cluster(size_t size) {
	// TODO доделать

	int optimal_rang = calculate_optimal_rang(size);
	char* res = first_stor.get_block(optimal_rang);

	if (res == nullptr) {
		res = add_pseudo_free_cluster_and_get_block(&first_stor, optimal_rang);
	}
	print("    after malloc\n");
	return res;
}

void free_block_in_clster(char *ptr) {
	cluster *c = get_begin_of_cluster(ptr);

    c->cluster_mutex.lock();

    c->free(ptr);

    // TODO что-нибудь с пустым cluster-ом

    if (c->)

    c->cluster_mutex.unlock();
}

char *realloc_block_in_cluster(char *ptr, size_t new_size) {
	size_t old_size = (1<<-cluster::get_rang(ptr)) - cluster::SERV_DATA_SIZE;
	cluster *c = get_begin_of_cluster(ptr);

	std::unique_lock<std::recursive_mutex> u_lock(c->cluster_mutex);

	if (new_size <= MAX_SIZE_TO_ALLOC_IN_SLAB) {
		char *res = alloc_block_in_slab(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		std::memcpy(res, ptr, std::min(new_size, old_size));
		c->free(ptr);
		return res;
	}

	if (new_size > MAX_SIZE_TO_ALLOC_IN_CLUSTERS) {
		char *res = alloc_big_block(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		std::memcpy(res, ptr, std::min(new_size, old_size));
		c->free(ptr);
		return res;
	}

	char *res = c->try_to_realloc(ptr, calculate_optimal_rang(new_size));
	if (res != nullptr) {
		return res;
	}

	u_lock.unlock();    // чтобы не было deadlock-а при alloc_block_in_cluster вместе с переразметкой
	res = alloc_block_in_cluster(new_size);

	if (res == nullptr) {
		u_lock.release();
		return nullptr;
	}
	
	std::memcpy(res, ptr, std::min(new_size, old_size));
	u_lock.lock();

	c->free(ptr);
	return res;
}
