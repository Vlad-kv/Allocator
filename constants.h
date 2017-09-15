#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <cstdio>

namespace {
	const int RANG_OF_CLUSTERS = 15;

	const size_t PAGE_SIZE = 4096;

	const int CLUSTER_MAX_RANG = 13;
	const int CLUSTER_MIN_RANG = 6;
	const int CLUSTER_SERV_DATA_SIZE = 8;

	const int MAX_SIZE_TO_ALLOC = 1<<30;
	const int MAX_SIZE_TO_ALLOC_IN_CLUSTERS = (1 << CLUSTER_MAX_RANG) - CLUSTER_SERV_DATA_SIZE;
	const int MAX_SIZE_TO_ALLOC_IN_SLAB = 0;

	static_assert((MAX_SIZE_TO_ALLOC_IN_SLAB <= MAX_SIZE_TO_ALLOC_IN_CLUSTERS) && (MAX_SIZE_TO_ALLOC_IN_CLUSTERS <= MAX_SIZE_TO_ALLOC), "invalid constants");
}

#endif
