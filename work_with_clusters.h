#ifndef WORK_WITH_CLUSTERS_H
#define WORK_WITH_CLUSTERS_H

#include "cluster.h"

void init_global_clusters_data();
char *alloc_if_clusters_not_fully_initialaised(size_t size);

char *alloc_block_in_cluster(size_t size);
void free_block_in_clster(char *ptr);
char *realloc_block_in_cluster(char *ptr, size_t new_size);
size_t malloc_usable_size_in_cluster(char *ptr);

namespace clusters {
	struct thread_data {
		storage_of_clusters local_storage;

		bool is_it_fully_initialaised = false;
		pthread_key_t local_key;
		storage_ptr local_storage_ptr;
	};
}
#endif
