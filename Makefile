
.PHONY: execute start_gitk simple_start_gitk

all: libAllocator.so

simple_start_gitk: libAllocatorSimple.so
	rm -f log_simple.txt
	LD_PRELOAD=./libAllocatorSimple.so gitk

start_gitk: libAllocator.so
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so gitk

execute: libAllocator.so test.exe
	rm -f log.txt
	LD_PRELOAD=./libAllocator.so ./test.exe

test.exe: test.cpp debug.h
	g++ -o test.exe test.cpp -std=c++11 -ldl


#work_with_slabs.h work_with_clusters.h work_with_clusters.cpp work_with_big_blocks.h
#storadge_of_clusters.h storadge_of_clusters.cpp debug.h debug.cpp
#constants.h cluster.h cluster.cpp Allocator.cpp


#work_with_slabs.cpp work_with_big_blocks.cpp
#work_with_clusters.cpp storadge_of_clusters.cpp debug.cpp cluster.cpp Allocator.cpp work_with_slabs.cpp work_with_big_blocks.cpp

libAllocator.so: work_with_slabs.h work_with_clusters.h work_with_clusters.cpp work_with_big_blocks.h storadge_of_clusters.h storadge_of_clusters.cpp debug.h debug.cpp constants.h cluster.h cluster.cpp Allocator.cpp work_with_slabs.cpp work_with_big_blocks.cpp
	g++ -shared -o libAllocator.so work_with_clusters.cpp storadge_of_clusters.cpp debug.cpp cluster.cpp Allocator.cpp work_with_slabs.cpp work_with_big_blocks.cpp -fPIC -ldl -std=c++11 -O2


clean:
	rm -f *.o *.exe *.so log.txt
