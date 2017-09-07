#include "storage_of_clusters.h"

using namespace std;

void storage_of_clusters::cut(cluster *c, int rang) { // только с блокировкой на storage_mutex и cluster_mutex!
	cluster *prev = c->prev_cluster;
	cluster *next = c->next_cluster;

	if (prev == nullptr) {
		clusters[rang] = next;
	} else {
		lock_guard<recursive_mutex> lg(prev->cluster_mutex);
		prev->next_cluster = next;
	}

	if (next != nullptr) {
		lock_guard<recursive_mutex> lg(next->cluster_mutex);
		next->prev_cluster = prev;
	}
}

void storage_of_clusters::overbalance(cluster *c) { // только с блокировкой на storage_mutex и cluster_mutex!
	if (c->max_available_rang != c->old_max_available_rang) {
		cut(c, c->old_max_available_rang);
		c->old_max_available_rang = c->max_available_rang;
		add_to_begin(c, c->max_available_rang);
	}
}

void storage_of_clusters::add_to_begin(cluster *c, int rang) { // только с блокировкой на storage_mutex и cluster_mutex!
	cluster *next = clusters[rang];
	{
		storage_ptr s_ptr;
		s_ptr.ptr = this;
		s_ptr.inc_use_count();

		c->this_storage_of_clusters = s_ptr;
	}
	if (next != nullptr) {
		lock_guard<recursive_mutex> lg(next->cluster_mutex);
		next->prev_cluster = c;
	}
	c->next_cluster = next;
	c->prev_cluster = nullptr;
	clusters[rang] = c;
}

void storage_of_clusters::init() {
	for (int w = MIN_RANG - 1; w <= MAX_RANG; w++) {
		clusters[w] = nullptr;
	}
}

char *storage_of_clusters::get_block(size_t rang) { // берёт storage_mutex
	my_assert((MIN_RANG <= rang) && (rang <= MAX_RANG), "incorrect rang in get_block");

	lock_guard<recursive_mutex> lg(storage_mutex);

	cluster *to_get = nullptr;

	for (int w = rang; w <= MAX_RANG; w++) {
		if (clusters[w] != nullptr) {
			to_get = clusters[w];
			to_get->cluster_mutex.lock();
			cut(to_get, w);
			break;
		}
	}

	if (to_get == nullptr) {
		return nullptr;
	}
	char *res = to_get->alloc(rang);
	to_get->old_max_available_rang = to_get->max_available_rang;

	add_to_begin(to_get, to_get->max_available_rang);

	to_get->cluster_mutex.unlock();
	return res;
}

void storage_of_clusters::add_cluster(cluster *c) { // берёт storage_mutex и cluster_mutex
	if (c == nullptr) {
		fatal_error("nullptr in storage_of_clusters::add_cluster\n");
	}
	lock_guard<recursive_mutex> lg(storage_mutex);
	lock_guard<recursive_mutex> lg_2(c->cluster_mutex);

	c->old_max_available_rang = c->max_available_rang;
	add_to_begin(c, c->max_available_rang);
}

//////////////////////////////////////////////////////////////////////////////

storage_ptr storage_ptr::create() {
	storage_ptr res;

	res.ptr = (storage_of_clusters*)mmap(nullptr, 1<<RANG_OF_CLUSTERS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	my_assert(res.ptr != MAP_FAILED, "mmap failed (in storage_ptr::create) with error : ", errno);

	new(res.ptr)storage_of_clusters();

	std::lock_guard<std::recursive_mutex> lg(res->storage_mutex);

	res->init();

	res.set_use_count(1);
	return res;
}

storage_ptr::storage_ptr()
: ptr(nullptr) {
}
storage_ptr::storage_ptr(storage_ptr& st_ptr)
: ptr(st_ptr.ptr) {
	if (ptr != nullptr) {
		inc_use_count();
	}
}
storage_ptr& storage_ptr::operator=(const storage_ptr& st_ptr) {
	release_ptr();
	ptr = st_ptr.ptr;
	if (ptr != nullptr) {
		inc_use_count();
	}
}

storage_ptr::~storage_ptr() {
	release_ptr();
}
storage_of_clusters* storage_ptr::operator->() {
	return ptr;
}
void storage_ptr::release_ptr() {
	if (ptr == nullptr) {
		return;
	}
	ptr->storage_mutex.lock();
	int count = get_use_count() - 1;
	set_use_count(count);
	ptr->storage_mutex.unlock();

	storage_of_clusters* old_ptr = ptr;
	ptr = nullptr;

	if (count == 0) {
		old_ptr->~storage_of_clusters();
		int res = munmap(old_ptr, 1<<12);
		my_assert(res == 0, "munmap failed with error : ", errno);
	}
}

long long storage_ptr::get_use_count() {
	return *((long long*)(((char*)ptr) + sizeof(storage_of_clusters)));
}
void storage_ptr::set_use_count(long long val) {
	*((long long*)(((char*)ptr) + sizeof(storage_of_clusters))) = val;
}
void storage_ptr::inc_use_count() {
	std::lock_guard<std::recursive_mutex> lg(ptr->storage_mutex);
	set_use_count(get_use_count() + 1);
}

bool operator==(const storage_ptr& p_1, const storage_ptr& p_2) {
	return (p_1.ptr == p_2.ptr);
}
bool operator!=(const storage_ptr& p_1, const storage_ptr& p_2) {
	return (p_1.ptr != p_2.ptr);
}
