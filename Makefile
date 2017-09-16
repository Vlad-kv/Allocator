
.PHONY: execute start_gitk s execute_test_2 start_nano start_subl start_ltrace start_ltrace_simple

all: libAllocator.so

simple_start_gitk: libAllocatorSimple.so
	rm -f log_simple.txt
	LD_PRELOAD=./libAllocatorSimple.so gitk

start_ltrace: libAllocator.so test.exe
	rm -f log.txt
	rm -f log_2.txt
	ltrace -f -o log_2.txt env LD_PRELOAD=./libAllocator.so ./test.exe

start_gitk: libAllocator.so
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so gitk

start_gitk_under_valgrind: libAllocator.so
	rm -f log.txt
	rm -f log_3.txt
	valgrind --tool=memcheck --trace-children=yes --log-file=log_3.txt env LD_PRELOAD=./libAllocator.so gitk

start_subl: libAllocator.so
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so subl

start_subl_under_valgrind: libAllocator.so
	rm -f log.txt
	rm -f log_3.txt
	valgrind --tool=helgrind --trace-children=yes --log-file=log_3.txt env LD_PRELOAD=./libAllocator.so subl

start_nano: libAllocator.so
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so nano

start_valgrind: libAllocator.so test.exe
	rm -f log_3.txt
	valgrind --tool=helgrind --trace-children=yes --log-file=log_3.txt env LD_PRELOAD=./libAllocator.so ./test.exe

execute: libAllocator.so test.exe
	rm -f log.txt
	env LD_PRELOAD=./libAllocator.so ./test.exe

test.exe: test.cpp
	g++ -g -O0 -o test.exe test.cpp -std=c++11 -ldl -lpthread

test_2.exe: test_2.cpp
	g++ -o test_2.exe test_2.cpp -std=c++11 -ldl

execute_test_2: test_2.exe libAllocator.so log_simple_2.txt
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so ./test_2.exe

libAllocator.so: work_with_big_blocks.h debug.h debug.cpp constants.h Allocator.cpp work_with_big_blocks.cpp work_with_slabs.cpp storage_of_clusters.cpp work_with_clusters.cpp cluster.cpp work_with_slabs.h storage_of_clusters.h work_with_clusters.h cluster.h
	g++ -g -O0 -shared -o libAllocator.so debug.cpp Allocator.cpp work_with_big_blocks.cpp work_with_slabs.cpp storage_of_clusters.cpp work_with_clusters.cpp cluster.cpp -fPIC -ldl -std=c++11 -O2 -lpthread


clean:
	rm -f *.o *.exe *.so log.txt log_3.txt log_2.txt
