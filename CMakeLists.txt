cmake_minimum_required(VERSION 3.10)
project(libperf-cpp VERSION 1.0.0 LANGUAGES CXX)

# perf-cpp requires complete std::from_chars support (GCC 11+, Clang 14+).
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
    message(FATAL_ERROR "perf-cpp requires GCC 11 or newer (found GCC ${CMAKE_CXX_COMPILER_VERSION}).")
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    message(FATAL_ERROR "perf-cpp requires Clang 14 or newer (found Clang ${CMAKE_CXX_COMPILER_VERSION}).")
endif()

set(CMAKE_CXX_STANDARD 17)

## Detect if perf-cpp is built standalone or embedded into another project (e.g., via add_subdirectory or FetchContent).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(PERF_CPP_IS_TOP_LEVEL ON)
else()
    set(PERF_CPP_IS_TOP_LEVEL OFF)
endif()

## Warning flags are applied per-target below (instead of via CMAKE_CXX_FLAGS) to keep them out of embedding projects.
set(PERF_CPP_WARNING_FLAGS
    -Wall -Wextra -Wpedantic -Wconversion -Wcast-align -Wunused -Wshadow -Wold-style-cast -Wpointer-arith
    -Wcast-qual -Wnon-virtual-dtor -Woverloaded-virtual -Wnull-dereference -Wdouble-promotion -Wformat=2
    -Wno-missing-braces)

## Only override build-type flags and the default build type when building standalone; embedding projects keep their own.
if(PERF_CPP_IS_TOP_LEVEL)
    set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -g -DNDEBUG")
    if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE "Release")
    endif()
    message("Building Mode                        = ${CMAKE_BUILD_TYPE}")
endif()

#############################################################
#   Options                                                 #
#############################################################
option(BUILD_EXAMPLES "Build examples in examples/ directory." OFF)
option(BUILD_LIB_SHARED "Build the library as a shared library (default is static)." OFF)
option(BUILD_TESTS "Build the unit tests." OFF)
option(GEN_PROCESSOR_EVENTS "Generate processor-specific events" OFF)

if(PERF_CPP_IS_TOP_LEVEL)
    message("Building as Shared Library           = ${BUILD_LIB_SHARED}")
    message("Building Examples                    = ${BUILD_EXAMPLES}")
    message("Building Tests                       = ${BUILD_TESTS}")
    message("Generating Processor-specific Events = ${GEN_PROCESSOR_EVENTS}")
endif()

#############################################################
#   perf-cpp source files                                   #
#############################################################
set(PERF_CPP_SRC
    src/config.cpp
    src/counter_definition.cpp
    src/event_counter.cpp
    src/event_file_descriptor_parser.cpp
    src/exception.cpp
    src/hardware_info.cpp
    src/sampler.cpp
    src/counter/counter.cpp
    src/counter/event_provider.cpp
    src/counter/group.cpp
    src/counter/requested_event.cpp
    src/counter/result.cpp
    src/sample/decoder.cpp
    src/sample/trigger.cpp
    src/sample/mmap_buffer.cpp
    src/sample/recording_values.cpp
    src/sample/record_file_writer.cpp
    src/sample/result.cpp
    src/metric/expression/token.cpp
    src/metric/expression/tokenizer.cpp
    src/metric/expression/parser.cpp
    src/metric/expression/function.cpp
    src/metric/expression/expression.cpp
    src/analyzer/memory_access.cpp
    src/analyzer/flame_graph_generator.cpp
    src/util/callchain_trie.cpp
    src/util/symbol_resolver.cpp
    src/util/table.cpp
)

#############################################################
#   Generated source file with processor-specific           #
#   events.                                                 #
#############################################################
if (GEN_PROCESSOR_EVENTS)
    set(PROCESSOR_EVENT_SRC_FILE "${CMAKE_CURRENT_SOURCE_DIR}/src/processor_specific_event_provider.cpp")

    # Execute the Python script to generate the file
    execute_process(
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/script/generate_processor_event_provider.py"
        --output "${PROCESSOR_EVENT_SRC_FILE}" --events "${CMAKE_CURRENT_SOURCE_DIR}/events" --verbose
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE SCRIPT_RESULT
    )

    # Verify the file was created; if yes, add the file and hint perf-cpp that this event provider
    # should be included into the default CounterDefinition.
    if(EXISTS "${PROCESSOR_EVENT_SRC_FILE}")
        set(PERF_CPP_SRC ${PERF_CPP_SRC} "${PROCESSOR_EVENT_SRC_FILE}")
        add_compile_definitions(PERFCPP_HAS_PROCESSOR_SPECIFIC_EVENTS)
    endif()
endif()

#############################################################
#   perf-cpp library definition                             #
#############################################################
if (BUILD_LIB_SHARED)
    add_library(perf-cpp SHARED ${PERF_CPP_SRC})
else ()
    add_library(perf-cpp STATIC ${PERF_CPP_SRC})
endif ()

## Attach usage requirements to the target so they travel with the installed export:
## the include directory (source tree when building, <prefix>/include when installed) and C++17.
include(GNUInstallDirs)
target_include_directories(perf-cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_compile_features(perf-cpp PUBLIC cxx_std_17)

## The sampler's overflow worker runs on a std::thread; PUBLIC so consumers linking the static library get the thread library, too.
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(perf-cpp PUBLIC Threads::Threads)

## Compile the library itself with the strict warning set; PRIVATE keeps the flags out of consuming targets.
target_compile_options(perf-cpp PRIVATE ${PERF_CPP_WARNING_FLAGS})

## Version the library; shared builds get a proper soname (e.g., libperf-cpp.so.1).
set_target_properties(perf-cpp PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

#############################################################
#   clang-tidy integration                                  #
#############################################################
## Lint alongside compilation, but only for standalone builds; embedding projects should not pay for (or fail on) linting.
if(PERF_CPP_IS_TOP_LEVEL)
    find_program(CLANG_TIDY NAMES "clang-tidy")
    if(CLANG_TIDY)
        set(CLANG_TIDY_COMMAND "${CLANG_TIDY}" "--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy")
        set_target_properties(perf-cpp PROPERTIES CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
    else()
        message(WARNING "clang-tidy not found")
    endif()
endif()

#############################################################
#   Examples definition                                     #
#############################################################
if(BUILD_EXAMPLES)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples/bin)
    include(examples/CMakeLists.txt)
endif()

#############################################################
#   Unit tests definition                                   #
#############################################################
if(BUILD_TESTS)
    include(test/CMakeLists.txt)
endif()

#############################################################
#   Maintainer-only custom targets (docs, perf-list);       #
#   not added when perf-cpp is embedded into another        #
#   project.                                                #
#############################################################
if(PERF_CPP_IS_TOP_LEVEL)
    ## To preview the documentation locally, run: .venv/bin/mkdocs serve
    find_program(MKDOCS_EXECUTABLE mkdocs HINTS ${CMAKE_CURRENT_SOURCE_DIR}/.venv/bin)
    if(MKDOCS_EXECUTABLE)
        add_custom_target(docs
                COMMAND ${MKDOCS_EXECUTABLE} build -f ${CMAKE_CURRENT_SOURCE_DIR}/mkdocs.yml -d ${CMAKE_CURRENT_SOURCE_DIR}/site
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                COMMENT "Building documentation website into site/"
        )
    endif()

    ## Extract hardware events into a CSV file.
    add_custom_target(perf-list python3 ${CMAKE_CURRENT_SOURCE_DIR}/script/create_perf_list.py)
endif()

#############################################################
#   Rules for installing the library                        #
#############################################################
include(CMakePackageConfigHelpers)

## Allow package maintainers to override the location of the CMake package config files.
set(PERF_CPP_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/perf-cpp"
    CACHE PATH "CMake package config location relative to the install prefix")
mark_as_advanced(PERF_CPP_INSTALL_CMAKEDIR)

## Install the library itself; the EXPORT clause records it for the package config.
install(
    TARGETS perf-cpp
    EXPORT perf-cppTargets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

## Install the public headers.
install(
    DIRECTORY include/
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.hpp" PATTERN "*.h"
)

## Generate a relocatable package config (no hardcoded install prefix).
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/perf-cppConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/perf-cppConfig.cmake"
    INSTALL_DESTINATION "${PERF_CPP_INSTALL_CMAKEDIR}"
)

## Generate the version file; consumers' find_package(perf-cpp 1.0) checks against this.
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/perf-cppConfigVersion.cmake"
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
        "${PROJECT_BINARY_DIR}/perf-cppConfig.cmake"
        "${PROJECT_BINARY_DIR}/perf-cppConfigVersion.cmake"
    DESTINATION "${PERF_CPP_INSTALL_CMAKEDIR}"
)

## Install the targets file that the config includes; defines the perf-cpp::perf-cpp target.
install(
    EXPORT perf-cppTargets
    NAMESPACE perf-cpp::
    DESTINATION "${PERF_CPP_INSTALL_CMAKEDIR}"
)