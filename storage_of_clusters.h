#ifndef STORAGE_OF_CLUSTERS_H
#define STORAGE_OF_CLUSTERS_H

class storage_of_clusters;

class storage_ptr {
public:
	friend class storage_of_clusters;
	friend void init_thread_local_cluster_data();

	friend bool operator==(const storage_ptr& p_1, const storage_ptr& p_2);
	friend bool operator!=(const storage_ptr& p_1, const storage_ptr& p_2);

	static storage_ptr create();

	storage_ptr();
	storage_ptr(const storage_ptr& st_ptr);
	storage_ptr& operator=(const storage_ptr& st_ptr);

	storage_of_clusters* operator->();
	void release_ptr();

	~storage_ptr();
// private:
	long long get_use_count();
	void set_use_count(long long val);
	void inc_use_count();

	storage_of_clusters* ptr;
};

#include "constants.h"
#include "cluster.h"

class storage_of_clusters {
public:
	static const int32_t MAX_RANG = CLUSTER_MAX_RANG;
	static const int32_t MIN_RANG = CLUSTER_MIN_RANG;

	friend void on_thread_exit(void *data);
private:
	cluster *clusters[MAX_RANG + 1];
public:
	std::recursive_mutex storage_mutex;
private:

	void add_to_begin(cluster *c, int rang); // только с блокировкой на storage_mutex и cluster_mutex!

public:
	void cut(cluster *c, int rang); // только с блокировкой на storage_mutex и cluster_mutex!

	void overbalance(cluster *c); // только с блокировкой на storage_mutex и cluster_mutex!

	void init();

	char *get_block(size_t rang); // берёт storage_mutex

	void add_cluster(cluster *c); // берёт storage_mutex и cluster_mutex

	storage_of_clusters() {
		print("in storage_of_clusters\n");
	}

	~storage_of_clusters() {
		print("in ~storage_of_clusters\n");
	}
};

static_assert(sizeof(storage_of_clusters) + 8 <= (1<<12), "too big storage_of_clusters");

bool operator==(const storage_ptr& p_1, const storage_ptr& p_2);
bool operator!=(const storage_ptr& p_1, const storage_ptr& p_2);

#endif
