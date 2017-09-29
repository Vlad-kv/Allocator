#include "work_with_slabs.h"

#define PAGE_SIZE 4096

#define MAX_SLAB_ALLOC 64

constexpr long long SLAB_MAGIC = 0x7777777777770000;

int slab_t_size[] = { 8, 16, 24, 32, 48, 64 };
int select_slab[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
		2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5 };

struct slab;

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

slab_free_pages pages;

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
	slab(unsigned char elem_size, thread_slab_storage * parent) {
		this->SLAB_MAGIC = 0x7777777777777777;
		this->elem_size = (unsigned char) elem_size;
		this->parent = parent;
		parent_alive = true;
		freest.push(reinterpret_cast<mem_node *>(data));
		freecnt.store(short(sizeof(data) / elem_size));
		next = nullptr;
		for (int i = elem_size; i + elem_size <= PAGE_SIZE - 40; i +=
				elem_size) {
			reinterpret_cast<mem_node *>(data + i - elem_size)->next.store(
					reinterpret_cast<mem_node *>(data + i));
		}
	}
	void * alloc_here() {
		void * res = reinterpret_cast<void *>(freest.pop());
		freecnt.fetch_sub(1);
		return res;
	}
	void free_here(void * ptr) {
		freest.push(reinterpret_cast<mem_node *>(ptr));
		int fcnt = freecnt.fetch_add(1);
		if (fcnt == 0 && parent_alive) {
			if (parent->alive.load()) {
				parent->slb[select_slab[elem_size]].push(this);
			} else {
				parent_alive = false;
			}
		} else if (!parent_alive && fcnt == int(sizeof(data) / elem_size) - 1) {
			pages.recycle_slab(this);
		}
	}
};

slab * slab_free_pages::get_slab(int elem_size, thread_slab_storage * parent) {
	std::lock_guard < std::mutex > lok(m);
	if (free_pages_count == 0) {
#ifdef _WIN64
		pages[free_pages_count++] = malloc(sizeof(slab));
#else
		pages[free_pages_count++] = mmap(nullptr, sizeof(slab),
		PROT_READ | PROT_WRITE,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#endif
	}
	parent->full.fetch_add(1);
	return new (pages[--free_pages_count]) slab(elem_size, parent);
}
void slab_free_pages::recycle_slab(slab * slab) {
	slab->parent->full.fetch_sub(1);
	slab->~slab();
	std::lock_guard < std::mutex > lok(m);
	if (free_pages_count < FREE_PAGES_MAX) {
		pages[free_pages_count++] = slab;
	} else {
#ifdef _WIN64
		free(slab);
#else
		munmap(slab, sizeof(slab));
#endif
	}
}

thread_slab_storage::thread_slab_storage() {
	full.store(0);
	alive.store(false);
}

thread_slab_storage::~thread_slab_storage() {
	assert(!alive.load());
}

void * thread_slab_storage::alloc_in_storage(size_t size) {
	int sslab = select_slab[size];
	slab * s = slb[sslab].pop();
	if (s == nullptr) {
		s = pages.get_slab(slab_t_size[sslab], this);
	}
	void * res = s->alloc_here();
	if (s->freecnt.load() != 0) {
		slb[sslab].push(s);
	}
	return res;
}

thread_slab_storage storages[200];

std::atomic_int cr_storage;

struct thread_slab_ptr {
	thread_slab_storage * storage;
	thread_slab_ptr() {
		storage = nullptr;
		while (storage == nullptr) {
			int id = cr_storage.fetch_add(1) % 200;
			if (!storages[id].alive.load() && storages[id].full.load() == 0) {
				storages[id].alive.store(true);
				storage = storages + id;
			}
		}
	}
	thread_slab_storage * operator()() {
		return storage;
	}
	~thread_slab_ptr() {
		storage->alive.store(false);
		for (int i = 0; i < 6; i++) {
			slab * s = nullptr;
			while ((s = storage->slb[i].pop()) != nullptr) {
				do {
					s->parent_alive = false;
					s = s->next.load();
				} while (s != nullptr);
			}
		}
	}
}
;

thread_local thread_slab_ptr my_storage;

void init_slab_allocation() {
	static_assert(sizeof(select_slab) == (MAX_SLAB_ALLOC + 1) * sizeof(int), "SLAB resolving array has wrong size");
	static_assert(sizeof(slab) == PAGE_SIZE, "SLAB block size not equal to page size");
}

char *alloc_block_in_slab(size_t size) {
	if (size > MAX_SLAB_ALLOC) {
		return nullptr;
	}
	return reinterpret_cast<char *>(my_storage()->alloc_in_storage(size));
}

constexpr long long PAGE_MASK = ~((long long) PAGE_SIZE - 1);

void free_block_in_slab(char *ptr) {
	slab * slb = reinterpret_cast<slab *>(reinterpret_cast<long long>(ptr)
			& PAGE_MASK);
	if ((slb->SLAB_MAGIC & (~65535LL)) == SLAB_MAGIC) {
		slb->free_here(ptr);
	}
}

char *realloc_block_in_slab(char *ptr, size_t new_size) {
	slab * slb = reinterpret_cast<slab *>(reinterpret_cast<long long>(ptr)
			& PAGE_MASK);
	if ((slb->SLAB_MAGIC & (~65535LL)) == SLAB_MAGIC) {
		if (new_size <= slb->elem_size) {
			return reinterpret_cast<char *>(ptr);
		} else {
			void * new_ptr = malloc(new_size);
			memcpy(new_ptr, ptr, slb->elem_size);
			free_block_in_slab(ptr);
			return reinterpret_cast<char *>(new_ptr);
		}
	} else {
		return nullptr;
	}
}

bool is_allocated_by_slab(void* ptr) {
	if (ptr == nullptr) {
		return false;
	}
	slab * slb = reinterpret_cast<slab *>(reinterpret_cast<long long>(ptr)
			& PAGE_MASK);
	return ((slb->SLAB_MAGIC & (~65535LL)) == SLAB_MAGIC);
}

size_t malloc_usable_size_in_slab(char* ptr) {
	if (ptr == nullptr) {
		return 0;
	}
	slab * slb = reinterpret_cast<slab *>(reinterpret_cast<long long>(ptr)
			& PAGE_MASK);
	if (((slb->SLAB_MAGIC & (~65535LL)) == SLAB_MAGIC)) {
		return slb->elem_size;
	} else {
		return 0;
	}
}
