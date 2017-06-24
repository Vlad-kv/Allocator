#ifndef STORADGE_OF_CLUSTERS_H
#define STORADGE_OF_CLUSTERS_H

class storadge_of_clusters;

#include "constants.h"
#include "cluster.h"

class storadge_of_clusters {
public:
	static const int32_t MAX_RANG = CLUSTER_MAX_RANG;
	static const int32_t MIN_RANG = CLUSTER_MIN_RANG;
private:
	cluster *clusters[MAX_RANG + 1];
	std::recursive_mutex storagde_mutex;

	void cut(cluster *c, int rang); // только с блокировкой на storagde_mutex!

	void add_to_begin(cluster *c, int rang); // только с блокировкой на storagde_mutex!

public:
	void init();

	char *get_block(size_t rang); // storagde_mutex

	void add_cluster(cluster *c); // берёт storagde_mutex и cluster_mutex
};

#endif
