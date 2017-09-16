#include <mutex>
#include <cstring>
#include <pthread.h>
#include <atomic>

#include "work_with_big_blocks.h"
#include "work_with_clusters.h"
#include "work_with_slabs.h"

#include "cluster.h"
#include "storage_of_clusters.h"
#include "constants.h"

using namespace std;
typedef unsigned long long ull;

const int MAX_NUMBER_OF_EMPTY_CLUSTERS = 1;
const int BUFFER_TO_LOCAL_KEY_SIZE = 64;

struct thread_data {
	storage_of_clusters local_storage;

	bool is_it_fully_initialaised = false;
	pthread_key_t local_key;
	storage_ptr local_storage_ptr;

	char buffer_to_local_key[BUFFER_TO_LOCAL_KEY_SIZE];
	size_t clusters_buffer_pos = 0;
};

static_assert(sizeof(thread_data) <= (1<<12), "too big thread_data");

thread_local thread_data *data = nullptr;

storage_ptr storage_of_clusters_without_owners_ptr;
mutex storage_of_clusters_without_owners_mutex;

int number_of_empty_clusters = 0;
cluster *first_empty_cluster = nullptr;
mutex empty_sorage_mutex;

storage_of_clusters storage_for_alloc_before_initialisation_completion;

cluster* get_empty_cluster();

void init_global_clusters_data() {
	lock_guard<mutex> lg(storage_of_clusters_without_owners_mutex);
	print("in init_global_clusters_data\n");

	storage_of_clusters_without_owners_ptr = storage_ptr::create();

	if (storage_of_clusters_without_owners_ptr.atom_ptr.load() == nullptr) {
		fatal_error("nullptr in storage_of_clusters_without_owners_ptr (in init_global_clusters_data)");
	}

	storage_for_alloc_before_initialisation_completion.init();
}

char *alloc_if_clusters_not_fully_initialaised(size_t size) {
	if ((data != nullptr) && (!data->is_it_fully_initialaised)) {
		size_t rang = calculate_optimal_rang(size);

		char *res = storage_for_alloc_before_initialisation_completion.get_block(rang);
		if (res == nullptr) {
			cluster *new_cluster = get_empty_cluster();
			
			if (new_cluster == nullptr) {
				fatal_error("impossible to alloc (in alloc_if_clusters_not_fully_initialaised)");
				return nullptr;
			}
			lock_guard<recursive_mutex> lg(new_cluster->cluster_mutex);

			res = new_cluster->alloc(rang);
			storage_for_alloc_before_initialisation_completion.add_cluster(new_cluster);
		}
		return res;
	}
	return nullptr;
}

cluster* get_empty_cluster() {
	lock_guard<mutex> lg(empty_sorage_mutex);
	if (number_of_empty_clusters == 0) {
		return create_cluster();
	}
	cluster* res = first_empty_cluster;
	first_empty_cluster = first_empty_cluster->next_cluster;
	number_of_empty_clusters--;

	return res;
}
void return_empty_cluster(cluster *c) {
	my_assert(c != nullptr, "nullptr in return_empty_cluster\n");
	lock_guard<mutex> lg(empty_sorage_mutex);

	if (number_of_empty_clusters == MAX_NUMBER_OF_EMPTY_CLUSTERS) {
		destroy_cluster(c);
		return;
	}
	c->next_cluster = first_empty_cluster;
	first_empty_cluster = c;
}

void on_thread_exit(void *v_data) {
	thread_data *data = (thread_data*)v_data;
	print("in on_thread_exit\n");

	if (data == nullptr) {
		fatal_error("nullptr in data (on_thread_exit)\n");
	}
	lock_guard<mutex> lg(storage_of_clusters_without_owners_mutex);
	unique_lock<recursive_mutex> ul(data->local_storage_ptr->storage_mutex);

	if (storage_of_clusters_without_owners_ptr.atom_ptr.load() == nullptr) {
		fatal_error("nullptr in storage_of_clusters_without_owners_ptr (on_thread_exit)\n");
	}

	for (int w = CLUSTER_MIN_RANG - 1; w <= CLUSTER_MAX_RANG; w++) {
		while (true) {
			cluster *c = data->local_storage_ptr->clusters[w];
			if (c == nullptr) {
				break;
			}
			data->local_storage_ptr->cut(c, w);

			storage_of_clusters_without_owners_ptr->add_cluster(c);
		}
		data->local_storage_ptr->clusters[w] = nullptr;
	}
	ul.unlock();
	ul.release();

	data->local_storage_ptr.release_ptr();
}

void init_thread_local_cluster_data() {
	if (data != nullptr) {
		// print("after init_thread_local_cluster_data (already init)\n\n");
		return;
	}
	
	print("in init_thread_local_cluster_data\n");
	
	char *ptr = (char*)mmap(nullptr, 1<<RANG_OF_CLUSTERS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	my_assert(ptr != MAP_FAILED, "mmap failed (in init_thread_local_cluster_data) with error : ", errno);

	new(ptr)thread_data();
	data = (thread_data*)ptr;

	int res = pthread_key_create(&data->local_key, on_thread_exit);
	if (res != 0) {
		fatal_error("pthread_key_create failed with error : ", res);
	}
	res = pthread_setspecific(data->local_key, data);
	if (res != 0) {
		fatal_error("pthread_setspecific failed with error : ", res);
	}
	data->local_storage_ptr.atom_ptr = (storage_of_clusters*)ptr;
	data->local_storage_ptr.inc_use_count();

	data->is_it_fully_initialaised = true;
	print("after init_thread_local_cluster_data (initialaised)\n\n");
}

char *add_free_cluster_and_get_block(int rang) {
	// print("In add_free_cluster_and_get_block\n");

	cluster *new_cluster = get_empty_cluster();
	if (new_cluster == nullptr) {
		return nullptr;
	}
	char *res;
	{
		lock_guard<recursive_mutex> lg(new_cluster->cluster_mutex);
		res = new_cluster->alloc(rang);
	}
	data->local_storage_ptr->add_cluster(new_cluster);
	return res;
}

cluster *get_begin_of_cluster(char *ptr) {
	char* aligned_ptr = (char*)(ptr - ((ull)ptr) % PAGE_SIZE);
	int num_pages = get_num_of_pages_to_begin(aligned_ptr) + 1;

	my_assert(num_pages <= 0, "invalid num_pages");
	return (cluster*)(aligned_ptr + num_pages * (ull)PAGE_SIZE);
}

char *alloc_block_in_cluster(size_t size) {
	init_thread_local_cluster_data();

	int optimal_rang = calculate_optimal_rang(size);
	char* res = data->local_storage_ptr->get_block(optimal_rang);

	if (res == nullptr) {
		res = add_free_cluster_and_get_block(optimal_rang);
	}
	return res;
}

void free_block_in_clster(char *ptr) {
	init_thread_local_cluster_data();

	cluster *c = get_begin_of_cluster(ptr);
    c->cluster_mutex.lock();

    c->free(ptr);

    bool b = c->is_empty();
    bool to_overbalance = c->is_necessary_to_overbalance();

    storage_ptr s_ptr(c->this_storage_of_clusters);

    c->cluster_mutex.unlock();

    if (b) { // вырезаю пустой cluster
    	lock_guard<recursive_mutex> lg(s_ptr->storage_mutex);
    	if (c->this_storage_of_clusters != s_ptr) {
    		return;
    	}
    	unique_lock<recursive_mutex> un_lock(c->cluster_mutex);
    	if (c->is_empty()) {
    		s_ptr->cut(c, c->old_max_available_rang);
    		c->old_max_available_rang = c->max_available_rang;

    		c->this_storage_of_clusters.release_ptr();

    		un_lock.unlock();
    		un_lock.release();

    		return_empty_cluster(c);
    		return;
    	}
    }

    if (to_overbalance) {
    	lock_guard<recursive_mutex> lg(s_ptr->storage_mutex);
    	if (c->this_storage_of_clusters != s_ptr) {
    		return;
    	}
    	lock_guard<recursive_mutex> lg_2(c->cluster_mutex);
    	s_ptr->overbalance(c);
    }
}

char *realloc_block_in_cluster(char *ptr, size_t new_size) {
	init_thread_local_cluster_data();

	size_t old_size = (1<<-cluster::get_rang(ptr - cluster::SERV_DATA_SIZE)) - cluster::SERV_DATA_SIZE;
	cluster *c = get_begin_of_cluster(ptr);

	unique_lock<recursive_mutex> u_lock(c->cluster_mutex);

	if (new_size <= MAX_SIZE_TO_ALLOC_IN_SLAB) {
		char *res = alloc_block_in_slab(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		memcpy(res, ptr, min(new_size, old_size));
		c->free(ptr);
		return res;
	}

	if (new_size > MAX_SIZE_TO_ALLOC_IN_CLUSTERS) {
		char *res = alloc_big_block(new_size);
		if (res == nullptr) {
			return nullptr;
		}
		memcpy(res, ptr, min(new_size, old_size));
		c->free(ptr);
		return res;
	}

	char *res = c->try_to_realloc(ptr, calculate_optimal_rang(new_size));
	if (res != nullptr) {
		return res;
	}

	u_lock.unlock();    // чтобы поддержать инвариант, что перед взятием storage_mutex-а поток
						// не владеет ни одним cluster_mutex-ом.
	res = alloc_block_in_cluster(new_size);

	if (res == nullptr) {
		u_lock.release();
		return nullptr;
	}

	memcpy(res, ptr, min(new_size, old_size));
	u_lock.lock();

	c->free(ptr);
	return res;
}

size_t malloc_usable_size_in_cluster(char *ptr) {
	ptr -= CLUSTER_SERV_DATA_SIZE;
	return (1 << (-cluster::get_rang(ptr))) - CLUSTER_SERV_DATA_SIZE;
}
