cmake_minimum_required(VERSION 2.8)

project(Allocator)

set(SOURCE_EXE test.cpp)
set(SOURCE_LIB Allocator.cpp)

set(CMAKE_CXX_FLAGS "-std=c++11")

add_library(Allocator SHARED ${SOURCE_LIB})
add_executable(test.exe ${SOURCE_EXE})
