cmake_minimum_required(VERSION 3.10)
project(libperf-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG")

include_directories(include/)

### Library
add_library(perf-cpp src/counter.cpp src/group.cpp src/counter_definition.cpp src/perf.cpp)

### Examples
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/bin)
add_executable(multi-thread examples/multi_thread.cpp)
target_link_libraries(multi-thread perf-cpp)
add_executable(single-thread examples/single_thread.cpp)
target_link_libraries(single-thread perf-cpp)