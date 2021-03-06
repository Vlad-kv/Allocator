#include <atomic>
#include <mutex>

#include "cluster.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

using namespace std;
using namespace clusters;

//----- Allocator.cpp --------------

atomic_bool is_initialized;
atomic_bool is_constructors_begin_to_executing;
mutex init_mutex;

//----- debug.cpp ------------------

std::recursive_mutex debug_mutex;
thread_local int num_alive_writers = 0;

//----- work_with_clusters.cpp -----

thread_local thread_data *data = nullptr;

storage_ptr storage_of_clusters_without_owners_ptr;
mutex storage_of_clusters_without_owners_mutex;

int number_of_empty_clusters = 0;
cluster *first_empty_cluster = nullptr;
mutex empty_sorage_mutex;

storage_of_clusters storage_for_alloc_before_initialisation_completion;

//----------------------------------

//----- work_with_slabs.cpp

slab_free_pages pages;
thread_slab_storage storages[200];
std::atomic_int cr_storage;
thread_local thread_slab_ptr my_storage;
// ....

//----- last object to initialize --

struct test_struct {
	test_struct() {
		is_constructors_begin_to_executing.store(true);
		is_initialized.store(false);
	}
};
test_struct test;
