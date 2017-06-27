#include "work_with_slabs.h"

#define PAGE_SIZE 4096
#define SLAB_MAGIC 0x7777
#define MAX_ACTIVE_THREADS 200

// 1 / 0.1
#define SIZE_UPDATE_BORDER 10

// 1 / 0.2
#define SLAB_RECLAIM_BORDER 5

#define SLAB_CHECKS

#ifdef SLAB_CHECKS
#include <cassert>
#endif

typedef short __int16_t;

// required mutex order :
// slab inside mutex -> global set mutex -> local set mutex -> slab outside mutex

namespace myslab {

    struct slab_data;

    struct slab_info {
        std::mutex mutex_inside; // for operations with elements of slab
        std::mutex mutex_outside; // for operations with slabs
        slab_info * next_slab = nullptr;
        const unsigned int elem_size;
        const int capacity;
        const int data_blocks_count;
        int size = 0;
        int last_submitted_size = 0;
        slab_data * assigned_store;
        void * next_free;
        slab_info(int elem_size, int block_count, slab_data * assigned,
                  void * next_free) :
                elem_size(elem_size), capacity(
                block_count * ((PAGE_SIZE - 4) / elem_size)), data_blocks_count(
                block_count), assigned_store(assigned), next_free(next_free) {
        }
    };

    template<size_t elem_size>
    struct slab {
        static constexpr int elements_per_block = (PAGE_SIZE - 4) / elem_size;

        struct slab_elem {
            union {
                char data[elem_size];
                slab_elem * next_elem;
            };
        };
        struct slab_data_block {
            union {
                char padding[PAGE_SIZE - 8];
                slab_elem elements[elements_per_block];
            };
            __int16_t magic[4] = { 0, 0, SLAB_MAGIC, 0 };
            slab_data_block() :
                    slab_data_block(nullptr) {
            }
            slab_data_block(slab_elem * last_next) {
                for (int i = 0; i < elements_per_block; i++) {
                    elements[i].next_elem = elements + i + 1;
                }
                elements[elements_per_block - 1].next_elem = last_next;
            }
        };

        union {
            char padding[PAGE_SIZE - 8];
            slab_info info;
        };
        __int16_t magic[4] = { 0, 0, SLAB_MAGIC, 0 };

        void mark_elems_initial(int data_blocks_count) {
            slab_data_block * dat = reinterpret_cast<slab_data_block *>(this);
            for (int i = 1; i < data_blocks_count; i++) {
                new (dat + i) slab_data_block(dat[i + 1].elements);
            }
            new (dat + data_blocks_count) slab_data_block();
        }

        slab(int data_blocks_count) :
                info(elem_size, data_blocks_count, nullptr,
                     reinterpret_cast<void *>(this + 1)) {
            mark_elems_initial(data_blocks_count);

            static_assert(elem_size >= 8, "elem_size should be >= 8");
            static_assert(elem_size % 8 == 0, "elem_size should be a multiple of 8");
            static_assert(sizeof(slab<elem_size>) == PAGE_SIZE, "slab is not page-sized");
            static_assert(sizeof(slab_data_block) == PAGE_SIZE, "slab data block is not page-sized");
            static_assert(sizeof(slab_elem) == elem_size, "because wtf");
        }

        void update_size(int diff);

        void * get_element() {
            std::lock_guard<std::mutex> lg(info.mutex_inside);
            if (info.size == info.capacity) {
                return nullptr;
            } else {
                slab_elem * next = reinterpret_cast<slab_elem *>(info.next_free);
                info.next_free = reinterpret_cast<void *>(next->next_elem);
                update_size(1);
                return reinterpret_cast<void *>(next);
            }
        }

        void reclaim_element(void * element) {
#ifdef SLAB_CHECKS
            assert(info.size > 0);
            long long elem = reinterpret_cast<long long>(element);
//		fprintf(stderr, "%llx %lld %lld\n", elem, sizeof(slab_elem), elem_size);
            assert((elem & (PAGE_SIZE - 1)) % sizeof(slab_elem) == 0);
            long long tis = reinterpret_cast<long long>(this);
            assert((elem - tis) / PAGE_SIZE >= 1);
            assert((elem - tis) / PAGE_SIZE <= info.data_blocks_count);
#endif
            std::lock_guard<std::mutex> lg(info.mutex_inside);
            slab_elem * it = reinterpret_cast<slab_elem *>(element);
            it->next_elem = reinterpret_cast<slab_elem *>(info.next_free);
            info.next_free = it;
            update_size(-1);
        }

        ~slab() {
        }
    };

    template<size_t elem_size>
    slab_info * create_slab(int min_capacity);

    struct slab_set {
        std::mutex mutex_set;
        slab_info * next_aval_slab = nullptr;
        int total_capacity = 0;
        int total_size = 0;
        template<size_t elem_size>
        slab_info * get_extra_slab(slab_data * assigner_data) {
            // TODO try to retrieve slab from global
            slab_info * res = create_slab<elem_size>(
                    std::max(200, total_capacity / 10));
            res->assigned_store = assigner_data;
            return res;
        }
        template<size_t elem_size>
        void * alloc_in_slabs(slab_data * assigner_data) {
//		fprintf(stderr, "start_alloc_slab\n");
            slab_info * current_slab = nullptr;
            std::unique_lock<std::mutex> set_lock(mutex_set);
            if (next_aval_slab == nullptr) {
                current_slab = next_aval_slab = get_extra_slab<elem_size>(
                        assigner_data);
//                fprintf(stderr, "new slab at %p\n", current_slab);
                total_capacity += current_slab->capacity;
            } else {
                current_slab = next_aval_slab;
            }
            set_lock.unlock();
//		fprintf(stderr, "start_slab_loop\n");
            for (;;) {
                slab<elem_size> * cslab =
                        reinterpret_cast<slab<elem_size> *>(current_slab);
                assert(cslab->magic[2] == SLAB_MAGIC);
                assert(!set_lock.owns_lock());
                void * try_alloc = cslab->get_element();
                if (try_alloc != nullptr) {
//				fprintf(stderr, "alloc at %p\n", try_alloc);
                    return try_alloc;
                } else {
                    std::lock(set_lock, current_slab->mutex_outside);
                    std::lock_guard<std::mutex>(current_slab->mutex_outside,
                                                std::adopt_lock);
                    next_aval_slab = current_slab->next_slab;
                    current_slab->next_slab = current_slab;
                    if (next_aval_slab == nullptr) {
                        current_slab = next_aval_slab = get_extra_slab<elem_size>(
                                assigner_data);
                        fprintf(stderr, "new slab at %p\n", current_slab);
                        total_capacity += current_slab->capacity;
                    }
                    current_slab = next_aval_slab;
                    set_lock.unlock();
                }
            }
            fprintf(stderr, "start new alloc");
            return alloc_in_slabs<elem_size>(assigner_data);
//		return nullptr;
        }
    };

    struct slab_data {
        slab_set s[6];
        int active = SLAB_MAGIC;
        slab_set * getset(const slab<8> * slab) {
            assert(slab->info.elem_size == 8);
            return s + 0;
        }
        slab_set * getset(const slab<16> * slab) {
            assert(slab->info.elem_size == 16);
            return s + 1;
        }
        slab_set * getset(const slab<24> * slab) {
            assert(slab->info.elem_size == 24);
            return s + 2;
        }
        slab_set * getset(const slab<32> * slab) {
            assert(slab->info.elem_size == 32);
            return s + 3;
        }
        slab_set * getset(const slab<48> * slab) {
            assert(slab->info.elem_size == 48);
            return s + 4;
        }
        slab_set * getset(const slab<64> * slab) {
            assert(slab->info.elem_size == 64);
            return s + 5;
        }
        slab_data() {
            // TODO constructor if needed
        }
        ~slab_data() {
            active = 0;
        }

    };

    template<size_t size_elem>
    void slab<size_elem>::update_size(int diff) {
        info.size += diff;
//	fprintf(stderr, "updated size\n");
        if (info.size - diff == info.capacity && info.next_slab == &info) {
            fprintf(stderr, "adding slab to queue\n");
            slab_set * st = info.assigned_store->getset(this);
            std::lock(st->mutex_set, info.mutex_outside);
            std::lock_guard<std::mutex> setlock(st->mutex_set, std::adopt_lock);
            std::lock_guard<std::mutex> outlock(info.mutex_outside,
                                                std::adopt_lock);
            info.next_slab = st->next_aval_slab;
            st->next_aval_slab = &info;
        }
        if (abs(info.size - info.last_submitted_size) * SIZE_UPDATE_BORDER
            >= info.capacity) {
//		fprintf(stderr, "diff is %d and capacity is %d\n",
//				info.size - info.last_submitted_size, info.capacity);
//		fprintf(stderr, "refreshing values\n");
            slab_set * st = info.assigned_store->getset(this);
            std::lock(st->mutex_set, info.mutex_outside);
//		fprintf(stderr, "locked all\n");
            std::lock_guard<std::mutex> setlock(st->mutex_set, std::adopt_lock);
            std::lock_guard<std::mutex> outlock(info.mutex_outside,
                                                std::adopt_lock);
            int idiff = info.size - info.last_submitted_size;
            st->total_size += idiff;
            info.last_submitted_size = info.size;
        }
    }

    slab_data * global_slab_data;
    size_t slab_data_last_index;

    pthread_key_t key;

    void * get_memory(int pages) {
        void * mmap_res = mmap(nullptr, PAGE_SIZE * pages, PROT_READ | PROT_WRITE, /*MAP_SHARED*/ MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if(mmap_res == MAP_FAILED){
            fatal_error("mmap failed\n");
        }
        return mmap_res;
//        return _aligned_malloc(pages * PAGE_SIZE, PAGE_SIZE);
//	return malloc(pages * PAGE_SIZE);
    }

    template<size_t elem_size>
    slab_info * create_slab(int min_capacity) {
        int data_blocks_cnt = (min_capacity - 1)
                              / slab<elem_size>::elements_per_block + 1;
        slab<elem_size> * ptr =
                new (get_memory(data_blocks_cnt + 1)) slab<elem_size>(
                        data_blocks_cnt);
        return &ptr->info;
    }

    slab_data * create_slab_data() {
        for (;;) {
            if (global_slab_data[slab_data_last_index].active != SLAB_MAGIC) {
                break;
            }
            if (slab_data_last_index == MAX_ACTIVE_THREADS) {
                slab_data_last_index = 1;
            } else {
                slab_data_last_index++;
            }
        }
        return new (global_slab_data + slab_data_last_index) slab_data();
    }

    slab_data * get_local_slab_data() {
        void * slab = pthread_getspecific(key);
        if (slab == nullptr) {
            slab = create_slab_data();
            pthread_setspecific(key, slab);
        }
        return reinterpret_cast<slab_data *>(slab);
    }

    slab_info * get_slab_from_ptr(char * ptr) {
        long long it = reinterpret_cast<long long>(ptr);
        it = it - (it & (PAGE_SIZE - 1)) - 4;
        __int16_t * p = reinterpret_cast<__int16_t *>(it);
        if (p[0] == SLAB_MAGIC) {
            p++;
            return reinterpret_cast<slab_info *>(it - PAGE_SIZE * p[0] - PAGE_SIZE
                                                 + 4);
        } else {
            return nullptr;
        }
    }

    void clean_slab_data(void * ptr) {
        reinterpret_cast<slab_data *>(ptr)->~slab_data();
    }

} // end of namespace;

using namespace myslab;

void init_slab_allocation() {
    pthread_key_create(&key, clean_slab_data);
    slab_data_last_index = 1;
    global_slab_data = reinterpret_cast<slab_data *>(get_memory(8));
    new (global_slab_data) slab_data();
}

char *alloc_block_in_slab(size_t size) {
//	fprintf(stderr, "start_alloc\n");
    slab_data * set_lc = get_local_slab_data();
    switch ((size - 1) / 8) {
        case 0: // 8
            return reinterpret_cast<char *>(set_lc->s[0].alloc_in_slabs<8>(set_lc));
        case 1: // 16
            return reinterpret_cast<char *>(set_lc->s[1].alloc_in_slabs<16>(set_lc));
        case 2: // 24
            return reinterpret_cast<char *>(set_lc->s[2].alloc_in_slabs<24>(set_lc));
        case 3: // 32
            return reinterpret_cast<char *>(set_lc->s[3].alloc_in_slabs<32>(set_lc));
        case 4:
        case 5:
            return reinterpret_cast<char *>(set_lc->s[4].alloc_in_slabs<48>(set_lc));
        case 6:
        case 7:
            return reinterpret_cast<char *>(set_lc->s[5].alloc_in_slabs<64>(set_lc));
        default:
            //	fatal_error("Not implemented");
            return nullptr;
    }
}

void free_block_in_slab(char *ptr, slab_info * info) {
    if (info == nullptr) {
        return;
    }
    switch ((info->elem_size - 1) / 8) {
        case 0:
            reinterpret_cast<slab<8>*>(info)->reclaim_element(
                    reinterpret_cast<void*>(ptr));
            break;
        case 1:
            reinterpret_cast<slab<16>*>(info)->reclaim_element(
                    reinterpret_cast<void*>(ptr));
            break;
        case 2:
            reinterpret_cast<slab<24>*>(info)->reclaim_element(
                    reinterpret_cast<void*>(ptr));
            break;
        case 3:
            reinterpret_cast<slab<32>*>(info)->reclaim_element(
                    reinterpret_cast<void*>(ptr));
            break;
        case 4:
        case 5:
            reinterpret_cast<slab<48>*>(info)->reclaim_element(
                    reinterpret_cast<void*>(ptr));
            break;
        case 6:
        case 7:
            reinterpret_cast<slab<64>*>(info)->reclaim_element(
                    reinterpret_cast<void*>(ptr));
            break;
        default:
            //	fatal_error("Not implemented");
            break;
    }
//	fatal_error("Not implemented");
}

void free_block_in_slab(char *ptr) {
    slab_info * info = get_slab_from_ptr(ptr);
    free_block_in_slab(ptr, info);
}

char *realloc_block_in_slab(char *ptr, size_t new_size) {
    slab_info * info = get_slab_from_ptr(ptr);
    if (info->elem_size >= new_size) {
        return ptr;
    } else {
        char * new_ptr = (char *)malloc(new_size);
        memcpy(new_ptr, ptr, info->elem_size);
        free_block_in_slab(ptr, info);
        return new_ptr;
    }
}
