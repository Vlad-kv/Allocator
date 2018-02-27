# Аллокатор - курсовой проект по курсу "Операционные системы".
Реализует основные функции стандартного аллокатора - malloc, free, calloc, realloc, posix_memalign, reallocarray и malloc_usable_size.

Многопоточен, эффективно использует с "большими" и "малыми" объектами - <= 64 - использует [slab allocation](https://en.wikipedia.org/wiki/Slab_allocation),
при 64 < size <= 1<<12 - 8 - использует отдельную структуру "claster", иначе - зыдействует напрямую mmap.

Для проверки на реальных программах используйте *LD_PRELOAD=./libAllocatorSimple.so xxx*, где xxx - имя бинарного файла.
