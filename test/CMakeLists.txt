Include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.14.0
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)


set(PERF_CPP_TEST
        test/access_benchmark.cpp
        test/config.cpp
        test/file_descriptor.cpp
        test/counter_config.cpp
        test/counter_definition.cpp
        test/counter_result.cpp
        test/metric.cpp
        test/event_counter.cpp
        test/event_counter_live.cpp
        test/event_counter_multi_thread.cpp
        test/event_counter_multi_core.cpp
        test/sampler.cpp
        test/sampler_multi_thread.cpp
        test/hardware_info.cpp
        test/requested_event.cpp
        test/sample_fields.cpp
        test/sample_result.cpp
        test/ibs_decoder.cpp
        test/directed_graph.cpp
        test/memory_access.cpp
        test/sample_recording_values.cpp
        test/table.cpp
        test/callchain_trie.cpp
        test/flame_graph_generator.cpp
        test/decoder.cpp
)

add_executable(tests ${PERF_CPP_TEST})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain perf-cpp)
target_include_directories(tests PRIVATE Catch2::Catch2WithMain test)
## Compile the tests with the library's strict warning set.
target_compile_options(tests PRIVATE ${PERF_CPP_WARNING_FLAGS})
## GCC 13 fires -Wnull-dereference inside <streambuf> when Catch2 uses istreambuf_iterator; suppress on tests only.
target_compile_options(tests PRIVATE $<$<CXX_COMPILER_ID:GNU>:-Wno-null-dereference>)
set_target_properties(tests
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

enable_testing()
catch_discover_tests(tests WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
