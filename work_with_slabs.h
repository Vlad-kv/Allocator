#ifndef WORK_WITH_SLABS_H
#define WORK_WITH_SLABS_H

#include <cstdio>
#include <mutex>
#include <atomic>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>
#include <cassert>

#include "constants.h"

void init_slab_allocation();

bool is_allocated_by_slab(void* ptr);
char *alloc_block_in_slab(size_t size);
void free_block_in_slab(char *ptr);
char *realloc_block_in_slab(char *ptr, size_t new_size);
size_t malloc_usable_size_in_slab(char *ptr);

template<class Node>
struct atomic_stack {
	std::atomic<Node *> m_Top;
	void push(Node * val) {

		Node * t = m_Top.load(std::memory_order_relaxed);
		while (true) {
			val->next.store(t, std::memory_order_relaxed);
			if (m_Top.compare_exchange_weak(t, val, std::memory_order_release,
					std::memory_order_relaxed))
				return;
		}
	}
	Node * pop() {
		while (true) {
			Node * t = m_Top.load(std::memory_order_relaxed);
			if (t == nullptr)
				return nullptr;  // stack is empty

			Node * pNext = t->next.load(std::memory_order_relaxed);
			if (m_Top.compare_exchange_weak(t, pNext, std::memory_order_acquire,
					std::memory_order_relaxed))
				return t;
		}
	}
};

struct slab;

struct thread_slab_storage {

	atomic_stack<slab> slb[6];
	std::atomic_int full;
	std::atomic<bool> alive;
	void * alloc_in_storage(size_t size);

	thread_slab_storage();
	~thread_slab_storage();
};

struct slab_free_pages {
	static const int FREE_PAGES_MAX = 200;
	std::mutex m;
	int free_pages_count;
	void * pages[FREE_PAGES_MAX];
	slab * get_slab(int elem_size, thread_slab_storage * parent);
	void recycle_slab(slab * slab);
};

struct slab {
	struct mem_node {
		std::atomic<mem_node *> next;
		mem_node();
		mem_node(mem_node * val) :
				next(val) {
		}
	};
	union {
		long long SLAB_MAGIC;
		struct {
			unsigned char elem_size;
			bool parent_alive;
		};
	};
//	std::atomic<void *> freeptr;
	atomic_stack<mem_node> freest;
	std::atomic_short freecnt;
	std::atomic<slab *> next;
	thread_slab_storage * parent;
	char data[PAGE_SIZE - 40];
	slab(unsigned char elem_size, thread_slab_storage * parent);
	void * alloc_here();
	void free_here(void * ptr);
};

struct thread_slab_ptr {
	thread_slab_storage * storage;
	thread_slab_ptr();
	thread_slab_storage * operator()();
	~thread_slab_ptr();
};

#endif
