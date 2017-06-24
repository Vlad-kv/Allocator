#ifndef WORK_WITH_CLUSTERS_H
#define WORK_WITH_CLUSTERS_H

#include "cluster.h"
#include "storadge_of_clusters.h"

char *alloc_block_in_cluster(size_t size);

void free_block_in_clster(char *ptr);

char *realloc_block_in_cluster(char *ptr, size_t new_size);

#endif
