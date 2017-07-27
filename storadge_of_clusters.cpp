#include "storadge_of_clusters.h"

void storadge_of_clusters::cut(cluster *c, int rang) { // только с блокировкой на storagde_mutex!
	cluster *prev = c->prev_cluster;
	cluster *next = c->next_cluster;

	if (prev == nullptr) {
		clusters[rang] = next;
	} else {
		std::lock_guard<std::recursive_mutex> lg(prev->cluster_mutex);
		prev->next_cluster = next;
	}

	if (next != nullptr) {
		std::lock_guard<std::recursive_mutex> lg(next->cluster_mutex);
		next->prev_cluster = prev;
	}
}

void storadge_of_clusters::add_to_begin(cluster *c, int rang) { // только с блокировкой на storagde_mutex!
	cluster *next = clusters[rang];
	c->this_storadge_of_clusters = this;

	if (next != nullptr) {
		std::lock_guard<std::recursive_mutex> lg(next->cluster_mutex);
		next->prev_cluster = c;
	}
	c->next_cluster = next;
	c->prev_cluster = nullptr;
	clusters[rang] = c;
}

void storadge_of_clusters::init() {
	for (int w = MIN_RANG - 1; w <= MAX_RANG; w++) {
		clusters[w] = nullptr;
	}
}

char *storadge_of_clusters::get_block(size_t rang) {
	my_assert((MIN_RANG <= rang) && (rang <= MAX_RANG), "incorrect rang in get_block");

	storagde_mutex.lock();
	cluster *to_get = nullptr;

	for (int w = rang; w <= MAX_RANG; w++) {
		if (clusters[w] != nullptr) {
			to_get = clusters[w];
			to_get->cluster_mutex.lock();
			cut(to_get, w);
			break;
		}
	}
	
	storagde_mutex.unlock();

	if (to_get == nullptr) {
		return nullptr;
	}
	char *res = to_get->alloc(rang);
	to_get->old_max_available_rang = to_get->max_available_rang;

	add_to_begin(to_get, to_get->max_available_rang);

	to_get->cluster_mutex.unlock();
	return res;
}

void storadge_of_clusters::add_cluster(cluster *c) { // берёт storagde_mutex и cluster_mutex
	if (c == nullptr) {
		// TODO
		fatal_error("not implemented\n");
	}

	std::lock_guard<std::recursive_mutex> lg(storagde_mutex);
	std::lock_guard<std::recursive_mutex> lg_2(c->cluster_mutex);

	c->old_max_available_rang = c->max_available_rang;
	add_to_begin(c, c->max_available_rang);
}
