cmake_minimum_required(VERSION 3.10)
project(libperf-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG")

### Get the underlying Linux Kernel version to check feature availability.
execute_process(COMMAND uname -r OUTPUT_VARIABLE UNAME_RESULT OUTPUT_STRIP_TRAILING_WHITESPACE)
string(REGEX MATCH "[0-9]+.[0-9]+" LINUX_KERNEL_VERSION ${UNAME_RESULT})

### Disable specific features if not provided by the Linux Kernel.
if (LINUX_KERNEL_VERSION VERSION_LESS 5.13)
    add_definitions(-DNO_PERF_COUNT_SW_CGROUP_SWITCHES)
endif()

if (LINUX_KERNEL_VERSION VERSION_LESS 5.12)
    add_definitions(-DNO_PERF_SAMPLE_WEIGHT_STRUCT)
endif()

if (LINUX_KERNEL_VERSION VERSION_LESS 5.11)
    add_definitions(-DNO_PERF_SAMPLE_DATA_PAGE_SIZE -DNO_PERF_SAMPLE_CODE_PAGE_SIZE)
endif()


### Add include directory.
include_directories(include/)

### Library
add_library(perf-cpp src/counter.cpp src/group.cpp src/counter_definition.cpp src/event_counter.cpp src/sampler.cpp)

### Examples
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/bin)

#### Single-threaded
add_executable(single-thread examples/single_thread.cpp examples/access_benchmark.cpp)
target_link_libraries(single-thread perf-cpp)

#### Multi-threaded; but inherit counter from main-thread
add_executable(inherit-thread examples/inherit_thread.cpp examples/access_benchmark.cpp)
target_link_libraries(inherit-thread perf-cpp)

#### Multi-threaded with thread-local counter
add_executable(multi-thread examples/multi_thread.cpp examples/access_benchmark.cpp)
target_link_libraries(multi-thread perf-cpp)

#### Sampling instruction pointers
add_executable(instruction-pointer-sampling examples/instruction_pointer_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(instruction-pointer-sampling perf-cpp)

#### Sampling instruction pointers
add_executable(counter-sampling examples/counter_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(counter-sampling perf-cpp)

#### Branch sampling
add_executable(branch-sampling examples/branch_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(branch-sampling perf-cpp)

#### Memory address sampling
add_executable(address-sampling examples/address_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(address-sampling perf-cpp)

#### Sampling user_registers
add_executable(register-sampling examples/register_sampling.cpp examples/access_benchmark.cpp)
target_link_libraries(register-sampling perf-cpp)

### Target to create the perf list CSV
add_custom_target(perf-list python3 ${CMAKE_SOURCE_DIR}/script/create_perf_list.py)