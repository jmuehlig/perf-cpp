cmake_minimum_required(VERSION 3.10)
project(libperf-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG")

include_directories(include/)

### Library
add_library(perf-cpp src/counter.cpp src/group.cpp src/counter_definition.cpp src/event_counter.cpp src/sampler.cpp)

### Examples
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/bin)
add_executable(multi-thread examples/multi_thread.cpp)
target_link_libraries(multi-thread perf-cpp)
add_executable(inherit-thread examples/inherit_thread.cpp)
target_link_libraries(inherit-thread perf-cpp)
add_executable(single-thread examples/single_thread.cpp)
target_link_libraries(single-thread perf-cpp)
add_executable(address-sampling examples/address_sampling.cpp)
target_link_libraries(address-sampling perf-cpp)
add_executable(group-sampling examples/group_sampling.cpp)
target_link_libraries(group-sampling perf-cpp)
add_executable(branch-sampling examples/branch_sampling.cpp)
target_link_libraries(branch-sampling perf-cpp)