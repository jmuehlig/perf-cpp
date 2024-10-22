cmake_minimum_required(VERSION 3.10)
project(libperf-cpp VERSION 0.7)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RELEASE)
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG")

### Add include directory.
include_directories(include/)

### Library
add_library(perf-cpp src/counter.cpp src/group.cpp src/counter_definition.cpp src/event_counter.cpp src/sampler.cpp src/analyzer/data.cpp)

### Examples
if(BUILD_EXAMPLES)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/bin)

    #### Single-threaded
    add_executable(single-thread EXCLUDE_FROM_ALL examples/single_thread.cpp examples/access_benchmark.cpp)
    target_link_libraries(single-thread perf-cpp)

    #### Multi-threaded; but inherit counter from main-thread
    add_executable(inherit-thread EXCLUDE_FROM_ALL examples/inherit_thread.cpp examples/access_benchmark.cpp)
    target_link_libraries(inherit-thread perf-cpp)

    #### Multi-threaded with thread-local counter
    add_executable(multi-thread EXCLUDE_FROM_ALL examples/multi_thread.cpp examples/access_benchmark.cpp)
    target_link_libraries(multi-thread perf-cpp)

    #### Multi-CPU with per-CPU counter
    add_executable(multi-cpu EXCLUDE_FROM_ALL examples/multi_cpu.cpp examples/access_benchmark.cpp)
    target_link_libraries(multi-cpu perf-cpp)

    #### Multi-Process with per-process counter
    add_executable(multi-process EXCLUDE_FROM_ALL examples/multi_process.cpp examples/access_benchmark.cpp)
    target_link_libraries(multi-process perf-cpp)

    #### Sampling instruction pointers
    add_executable(instruction-pointer-sampling EXCLUDE_FROM_ALL examples/instruction_pointer_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(instruction-pointer-sampling perf-cpp)

    #### Sampling instruction pointers
    add_executable(counter-sampling EXCLUDE_FROM_ALL examples/counter_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(counter-sampling perf-cpp)

    #### Branch sampling
    add_executable(branch-sampling EXCLUDE_FROM_ALL examples/branch_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(branch-sampling perf-cpp)

    #### Memory address sampling
    add_executable(address-sampling EXCLUDE_FROM_ALL examples/address_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(address-sampling perf-cpp)

    #### Sampling user_registers
    add_executable(register-sampling EXCLUDE_FROM_ALL examples/register_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(register-sampling perf-cpp)

    #### Sampling on multiple threads
    add_executable(multi-thread-sampling EXCLUDE_FROM_ALL examples/multi_thread_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(multi-thread-sampling perf-cpp)

    #### Sampling on multiple threads
    add_executable(multi-cpu-sampling EXCLUDE_FROM_ALL examples/multi_cpu_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(multi-cpu-sampling perf-cpp)

    #### Sampling with multiple events
    add_executable(multi-event-sampling EXCLUDE_FROM_ALL examples/multi_event_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(multi-event-sampling perf-cpp)

    #### Sampling with raw values
    add_executable(amd-ibs-raw-sampling EXCLUDE_FROM_ALL examples/amd_ibs_raw_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(amd-ibs-raw-sampling perf-cpp)

    #### Sampling with raw values
    add_executable(context-switch-sampling EXCLUDE_FROM_ALL examples/context_switch_sampling.cpp examples/access_benchmark.cpp)
    target_link_libraries(context-switch-sampling perf-cpp)

    #### Analyze Samples with DataAnalyzer
    add_executable(data-analyzer EXCLUDE_FROM_ALL examples/data_analyzer.cpp examples/access_benchmark.cpp)
    target_link_libraries(data-analyzer perf-cpp)

    ### One target for all examples
    add_custom_target(examples)
    add_dependencies(examples
            single-thread inherit-thread multi-thread multi-cpu multi-process
            instruction-pointer-sampling counter-sampling branch-sampling
            address-sampling register-sampling multi-thread-sampling multi-cpu-sampling
            multi-event-sampling amd-ibs-raw-sampling context-switch-sampling data-analyzer)
endif()

### Target to create the perf list CSV
add_custom_target(perf-list python3 ${CMAKE_SOURCE_DIR}/script/create_perf_list.py)

### Install rules
if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()

# install rules
if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_INSTALL_INCLUDEDIR include CACHE PATH "")
  set(CMAKE_INSTALL_BINARY_DIR bin CACHE PATH "")
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
# Allow package maintainers to freely override the path for the configs
set(package perf-cpp)

install(DIRECTORY include/ DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}" COMPONENT perf-cpp_Development)
install(TARGETS perf-cpp EXPORT perf-cppTargets INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

if(BUILD_EXAMPLES)
    install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/examples/bin/ DESTINATION "${CMAKE_INSTALL_BINARY_DIR}" USE_SOURCE_PERMISSIONS)
endif()

write_basic_package_version_file(
    "${package}ConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
    ARCH_INDEPENDENT
)

set(
    perf-cpp_INSTALL_CMAKEDIR "${CMAKE_INSTALL_DATADIR}/${package}"
    CACHE PATH "CMake package config location relative to the install prefix"
)
mark_as_advanced(perf-cpp_INSTALL_CMAKEDIR)

write_file(
    "${PROJECT_BINARY_DIR}/${package}Config.cmake"
    "include(\"${CMAKE_INSTALL_PREFIX}/${perf-cpp_INSTALL_CMAKEDIR}/${package}Targets.cmake\")"
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}Config.cmake"
    DESTINATION "${perf-cpp_INSTALL_CMAKEDIR}"
    COMPONENT perf-cpp_Development
)

install(
    FILES "${PROJECT_BINARY_DIR}/${package}ConfigVersion.cmake"
    DESTINATION "${perf-cpp_INSTALL_CMAKEDIR}"
    COMPONENT perf-cpp_Development
)

install(
    EXPORT perf-cppTargets
    NAMESPACE perf-cpp::
    DESTINATION "${perf-cpp_INSTALL_CMAKEDIR}"
    COMPONENT perf-cpp_Development
)
