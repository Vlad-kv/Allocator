#include "cluster.h"

#include <atomic>

using namespace std;
typedef unsigned long long ull;

int32_t cluster::get_rang(char *ptr) {
	return (*(atomic<int32_t>*)(ptr)).load();
}
void cluster::set_rang(char *ptr, int32_t val) {
	(*(atomic<int32_t>*)(ptr)).store(val);
	(*(atomic<int32_t>*)(ptr + sizeof(int32_t))).store(-(ptr - storage) / PAGE_SIZE - 1);
}
char* cluster::get_prev(char *ptr) {
	return *(char**)(ptr + sizeof(void*));
}
void cluster::set_prev(char *ptr, char* val) {
	*(char**)(ptr + sizeof(void*)) = val;
}
char* cluster::get_next(char *ptr) {
	return *(char**)(ptr + 2 * sizeof(void*));
}
char* cluster::set_next(char *ptr, char* val) {
	*(char**)(ptr + 2 * sizeof(void*)) = val;
}

void cluster::cut(char* block) {
	// print("  In cut: ", block - storage, "\n");
	char* prev = get_prev(block);
	char* next = get_next(block);
	
	if ((MIN_RANG <= storage - prev) && (storage - prev <= MAX_RANG)) {
		levels[storage - prev] = next;
	} else {
		set_next(prev, next);
	}
	if (next != nullptr) {
		set_prev(next, prev);
	}
}

void cluster::add_to_begin(int level, char* block) {
	my_assert(level == get_rang(block));
	set_next(block, levels[level]);
	set_prev(block, storage - level);

	if (levels[level] != nullptr) {
		set_prev(levels[level], block);
	}
	levels[level] = block;
}

char* cluster::split(char* block, ll neded_level) {
	// print("  In split\n");
	ll level = get_rang(block);
	cut(block);
	while (level > neded_level) {
		level--;
		char* twin = block + (1<<level);
		set_rang(twin, level);

		add_to_begin(level, twin);
	}
	return block;
}

cluster::cluster() 
: storage((char*)this) {
	print(" In constructor \n        ##########\n        ##########\n        ##########\n\n");

	lock_guard<recursive_mutex> lg(cluster_mutex);

	max_available_rang = MAX_RANG;

	available_memory = (1<<RANG_OF_CLUSTERS) - (1<<RESERVED_RANG);
	for (int w = MIN_RANG; w <= MAX_RANG; w++) {
		levels[w] = nullptr;
	}
	for (int w = (1<<RANG_OF_CLUSTERS) - (1<<MAX_RANG); w > 0; w -= (1<<MAX_RANG)) {
		set_rang(storage + w, MAX_RANG);
		add_to_begin(MAX_RANG, storage + w);
	}
	for (int w = MAX_RANG - 1; w >= RESERVED_RANG; w--) {
		char *twin = storage + (1<<w);
		set_rang(twin, w);
		add_to_begin(w, twin);
	}
	set_rang(storage, -RESERVED_RANG);
}

char *cluster::alloc(size_t optimal_level) {
	// print("In alloc: ", optimal_level, "\n");

	int level = optimal_level;

	my_assert(level <= MAX_RANG);
	while ((level <= MAX_RANG) && (levels[level] == nullptr)) {
		level++;
	}
	if (level > MAX_RANG) {
		// fatal_error("return nullptr\n");
		
		print(" return nullptr in alloc\n");
		return nullptr;
	}

	available_memory -= 1<<optimal_level;
	
	char* to_alloc = split(levels[level], optimal_level);
	set_rang(to_alloc, -optimal_level);

	update_max_available_rang();

	// print(" allocated: ", to_alloc - storage, " ", to_alloc - storage + (1<<optimal_level),  "\n");
	// print_state();
	// print("----------------------------------\n\n");
	// print("                        ", available_memory, "\n");
	return to_alloc + SERV_DATA_SIZE;
}

void cluster::free(char* ptr) {
	if (ptr == nullptr) {
		return;
	}
	ptr -= SERV_DATA_SIZE;

	my_assert(is_valid_ptr(ptr), "not valid ptr in free");

	// print("In free: \n");
	ll block_rang = -get_rang(ptr);

	available_memory += 1<<block_rang;

	my_assert(is_valid_rang(block_rang), "incorrect rang");
	// print(" ", ptr - storage, " ", ptr - storage + (1<<block_rang), "\n");
	while (block_rang < MAX_RANG) {
		char* twin = ((ptr - storage) ^ (1<<block_rang)) + storage;

		if ((twin == storage) || (get_rang(twin) != block_rang)) {
			break;
		}
		cut(twin);
		ptr = std::min(ptr, twin);
		block_rang++;
	}
	set_rang(ptr, block_rang);
	
	add_to_begin(block_rang, ptr);

	update_max_available_rang();

	// print_state();
	// print("------------------------\n");
	// print("                        ", available_memory, "\n");
}

char *cluster::try_to_realloc(char *ptr, size_t new_rang_of_block) {
	// TODO реализовать честную попытку расширить на месте
	return nullptr;
}

bool cluster::is_empty() {
	return (available_memory == (1<<RANG_OF_CLUSTERS) - (1<<RESERVED_RANG));
}

cluster *create_cluster() {
	char *res = (char*)mmap(nullptr, 1<<RANG_OF_CLUSTERS, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	if (res == MAP_FAILED) {
		return nullptr;
	}

	new(res)cluster();
	return (cluster*)res;
}
void destroy_cluster(cluster *c) {
	my_assert(c != nullptr, "nullptr in destroy_cluster");
	int error = munmap(c, 1<<RANG_OF_CLUSTERS);
	my_assert(error == 0, "error in munmup in destroy_cluster: ", errno);
}

int32_t get_num_of_pages_to_begin(char *ptr) {
	return (*(atomic<int32_t>*)(ptr + sizeof(int32_t))).load();
}

int calculate_optimal_rang(size_t size) {
	for (int w = cluster::MIN_RANG; w <= cluster::MAX_RANG; w++) {
		if ((1<<w) - cluster::SERV_DATA_SIZE >= size) {
			return w;
		}
	}
	return -1;
}
