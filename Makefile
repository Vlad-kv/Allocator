
.PHONY: execute start_gitk s execute_test_2

all: libAllocator.so

simple_start_gitk: libAllocatorSimple.so
	rm -f log_simple.txt
	LD_PRELOAD=./libAllocatorSimple.so gitk

start_gitk: libAllocator.so
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so gitk

start_subl: libAllocator.so
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so subl

execute: libAllocator.so test.exe
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so ./test.exe

test.exe: test.cpp
	g++ -o test.exe test.cpp -std=c++11 -ldl

test_2.exe: test_2.cpp
	g++ -o test_2.exe test_2.cpp -std=c++11 -ldl

execute_test_2: test_2.exe libAllocator.so log_simple_2.txt
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so ./test_2.exe

#work_with_slabs.h work_with_clusters.h work_with_clusters.cpp work_with_big_blocks.h
#storadge_of_clusters.h storadge_of_clusters.cpp debug.h debug.cpp
#constants.h cluster.h cluster.cpp Allocator.cpp


#work_with_slabs.cpp work_with_big_blocks.cpp
#work_with_clusters.cpp storadge_of_clusters.cpp debug.cpp cluster.cpp Allocator.cpp work_with_slabs.cpp work_with_big_blocks.cpp

libAllocator.so: work_with_slabs.h work_with_clusters.h work_with_clusters.cpp work_with_big_blocks.h storadge_of_clusters.h storadge_of_clusters.cpp debug.h debug.cpp constants.h cluster.h cluster.cpp Allocator.cpp work_with_slabs.cpp work_with_big_blocks.cpp
	g++ -shared -o libAllocator.so work_with_clusters.cpp storadge_of_clusters.cpp debug.cpp cluster.cpp Allocator.cpp work_with_slabs.cpp work_with_big_blocks.cpp -fPIC -ldl -std=c++11 -O2 -lpthread


clean:
	rm -f *.o *.exe *.so log.txt
