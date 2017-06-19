#ifndef CLUSTER_H
#define CLUSTER_H

#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <sys/mman.h>
#include <cstring>

#include "debug.h"

struct test_struct {
	int val;
	test_struct(int val)
	: val(val) {
	}
};

class cluster {
private:
	static const int32_t MAX_RANG = 29;
	static const int32_t MIN_RANG = 6;
	static const int32_t SHIFT = 16;
	static const int32_t SERV_DATA_SIZE = 8;

	typedef long long ll;

	bool is_initialized_str = 0;

	char *storage;
	int32_t rang;
	char* levels[MAX_RANG + 1];
	
	ll get_rang(char *ptr) {
		return *(ll*)ptr;
	}
	void set_rang(char *ptr, ll val) {
		*(ll*)ptr = val;
	}
	char* get_prev(char *ptr) {
		return *(char**)(ptr + 1 * sizeof(ll));
	}
	void set_prev(char *ptr, char* val) {
		*(char**)(ptr + 1 * sizeof(ll)) = val;
	}
	char* get_next(char *ptr) {
		return *(char**)(ptr + 2 * sizeof(ll));
	}
	char* set_next(char *ptr, char* val) {
		*(char**)(ptr + 2 * sizeof(ll)) = val;
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
		// print("  ", (ll)(prev - storage), " ", (ll)(next - storage), "\n");

		if (prev < storage) {
			levels[storage - prev] = next;
		} else {
			set_next(prev, next);
		}
		if (next != nullptr) {
			set_prev(next, prev);
		}
		// print("  After cut\n");
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
		ll level = get_rang(block);
		cut(block);
		while (level > neded_level) {
			level--;
			char* twin = block + (1<<level);
			set_rang(twin, level);
			// print("cutted twin: ", twin - storage, "\n");

			add_to_begin(level, twin);
		}
		return block;
	}

public:

	bool is_initialized() {
		return is_initialized_str;
	}

	void init(int32_t rang) {
		my_assert(!is_initialized_str);
		// print("In init   \n##########\n##########\n##########\n###########\n\n\n\n");

		is_initialized_str = true;
		this->rang = rang;

		my_assert((MIN_RANG <= rang) && (rang <= MAX_RANG));
		
		storage = (char*)mmap(nullptr, 1<<rang, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		for (int w = MIN_RANG; w <= rang; w++) {
			levels[w] = nullptr;
		}
		levels[rang] = storage;
		
		set_rang(storage, rang);
		set_prev(storage, storage - rang);
		set_next(storage, nullptr);
	}

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
			print("return nullptr\n");
			return nullptr;
		}
		char* to_alloc = split(levels[level], optimal_level);
		set_rang(to_alloc, -optimal_level);

		// print(" allocated: ", to_alloc - storage, " ", to_alloc - storage + (1<<optimal_level),  "\n");
		// print_state();
		// print("----------------------------------\n\n");
		return to_alloc + SERV_DATA_SIZE;
	}

	void free(char* ptr) {
		if (ptr == nullptr) {
			return;
		}
		my_assert(is_valid_ptr(ptr), "not valid ptr in free");

		// print("In free: \n");
		ptr -= 8;
		ll block_rang = -get_rang(ptr);

		my_assert(is_valid_rang(block_rang), "incorrect rang");
		// print(" ", ptr - storage, " ", ptr - storage + (1<<block_rang), "\n");

		while (block_rang < rang) {
			char* twin = ((ptr - storage) ^ (1<<block_rang)) + storage;
			if (get_rang(twin) != block_rang) {
				break;
			}
			cut(twin);
			ptr = std::min(ptr, twin);
			block_rang++;
		}
		set_rang(ptr, block_rang);
		add_to_begin(block_rang, ptr);
		// print("------------------------\n");
	}

	char *realloc(char *ptr, size_t size) {
		if (ptr == nullptr) {
			return alloc(size);
		}
		my_assert(is_valid_ptr(ptr), "not valid ptr in realloc");
		// print("In realloc:\n");

		my_assert(is_valid_rang(-get_rang(ptr - SERV_DATA_SIZE)), "incorrect rang");

		size_t old_size = 1 << -get_rang(ptr - SERV_DATA_SIZE);

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

	~cluster() {
		munmap((void*)storage, 1<<rang);
	}
};

#endif // CLUSTER_H
