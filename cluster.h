#ifndef CLUSTER_H
#define CLUSTER_H

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <sys/mman.h>
#include <cstring>

#include "debug.h"

class cluster {
public:
	static const int32_t MAX_RANG = 29;
	static const int32_t MIN_RANG = 6;
	static const int32_t SHIFT = 16;
	static const int32_t SERV_DATA_SIZE = 8;
	static const int32_t PAGE_SIZE = 4096;
	static const int32_t RESERVED_RANG = 9;
private:
	typedef long long ll;

	char *storage;
	int32_t rang;
	char* levels[MAX_RANG + 1];
	ll available_memory = 0;

	ll get_rang(char *ptr) {
		return *(ll*)(ptr - sizeof(void*));
	}
	void set_rang(char *ptr, ll val) {
		*(ll*)(ptr - sizeof(void*)) = val;
	}
	char* get_prev(char *ptr) {
		return *(char**)ptr;
	}
	void set_prev(char *ptr, char* val) {
		*(char**)ptr = val;
	}
	char* get_next(char *ptr) {
		return *(char**)(ptr + sizeof(void*));
	}
	char* set_next(char *ptr, char* val) {
		*(char**)(ptr + sizeof(void*)) = val;
	}

	bool is_valid_ptr(char *ptr) {// debug
		return ((storage <= ptr) && (ptr < storage + (1 << rang)));
	}
	bool is_valid_rang(int rang) {
		return ((MIN_RANG <= rang) && (rang <= this->rang));
	}

	void print_state() {// debug
		for (int w = rang; w >= MIN_RANG; w--) {
			print(w, " : ");
			if (levels[w] != nullptr) {
				print(get_prev(levels[w]) - storage, "\n");
				my_assert((get_prev(levels[w]) - storage) == -w, "incorrect level");
			}
			for (char* e = levels[w]; e != nullptr; e = get_next(e)) {
				my_assert((MIN_RANG <= get_rang(e)) && (get_rang(e) <= rang), "incorrect rang");

				print(e - storage, " ", e - storage + (1<<w), " | ");
			}
			print("\n");
		}
		print("\n");
	}

	void cut(char* block) {
		// print("  In cut: ", block - storage, "\n");
		char* prev = get_prev(block);
		char* next = get_next(block);
		
		if ((MIN_RANG <= storage - prev) && (storage - prev <= rang)) {
			levels[storage - prev] = next;
		} else {
			set_next(prev, next);
		}
		if (next != nullptr) {
			set_prev(next, prev);
		}
	}

	void add_to_begin(int level, char* block) {
		my_assert(level == get_rang(block));
		set_next(block, levels[level]);
		set_prev(block, storage - level);

		if (levels[level] != nullptr) {
			set_prev(levels[level], block);
		}
		levels[level] = block;
	}

	char* split(char* block, ll neded_level) {
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

	cluster(int rang) 
	: rang(rang) {
		print(" In constructor \n        ##########\n        ##########\n        ##########\n\n");

		my_assert((MIN_RANG <= rang) && (rang <= MAX_RANG), "invalid rang");
		storage = (char*)this;

		available_memory = 1<<rang;
		for (int w = MIN_RANG; w <= rang; w++) {
			levels[w] = nullptr;
		}
		for (int w = rang - 1; w >= RESERVED_RANG; w--) {
			char *twin = storage + (1<<w);
			set_rang(twin, w);
			add_to_begin(w, twin);
		}
	}
public:

	char *alloc(size_t size) {
		// print("In alloc: ", size, "\n");
		
		int power = 1<<MIN_RANG;
		int level = MIN_RANG;
		while (power - SERV_DATA_SIZE < size) {
			level++;
			power <<= 1;
		}
		int optimal_level = level;
		// print(" optimal_level: ", optimal_level, "\n");
		my_assert(level <= rang);
		while ((level <= rang) && (levels[level] == nullptr)) {
			level++;
		}
		if (level > rang) {
			fatal_error("return nullptr\n");
			// print("return nullptr\n");
			// return nullptr;
		}
		available_memory -= 1<<optimal_level;
		char* to_alloc = split(levels[level], optimal_level);
		set_rang(to_alloc, -optimal_level);

		// print(" allocated: ", to_alloc - storage, " ", to_alloc - storage + (1<<optimal_level),  "\n");
		// print_state();
		// print("----------------------------------\n\n");
		print("                        ", available_memory, "\n");
		return to_alloc;
	}

	void free(char* ptr) {
		if (ptr == nullptr) {
			return;
		}
		my_assert(is_valid_ptr(ptr), "not valid ptr in free");

		// print("In free: \n");
		ll block_rang = -get_rang(ptr);

		available_memory += 1<<block_rang;

		my_assert(is_valid_rang(block_rang), "incorrect rang");
		// print(" ", ptr - storage, " ", ptr - storage + (1<<block_rang), "\n");
		while (block_rang < rang) {
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
		// print("------------------------\n");
		print("                        ", available_memory, "\n");
	}

	char *realloc(char *ptr, size_t size) {
		if (ptr == nullptr) {
			return alloc(size);
		}
		my_assert(is_valid_ptr(ptr), "not valid ptr in realloc");
		// print("In realloc:\n");

		my_assert(is_valid_rang(-get_rang(ptr)), "incorrect rang");

		size_t old_size = (1 << -get_rang(ptr)) - 8;

		char *res = alloc(size);
		if (res == nullptr) {
			return nullptr;
		}
		for (int w = std::min(size, old_size) - 1; w >= 0; w--) {
			res[w] = ptr[w];
		}
		free(ptr);

		// print_state();
		// print("reallocated : ", res - storage, "\n");
		// print("----------------------\n");

		return res;
	}

	friend cluster* create_cluster(int rang);
};

static_assert((1<<cluster::RESERVED_RANG) - cluster::SERV_DATA_SIZE >= sizeof(cluster), "too small RESERVED_RANG");

cluster *create_cluster(int rang) {
	char *res = (char*)mmap(nullptr, 1<<rang, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (res == MAP_FAILED) {
		fatal_error("mmap failed\n");
	}
	new(res)cluster(rang);
	return (cluster*)res;
}

#endif // CLUSTER_H
