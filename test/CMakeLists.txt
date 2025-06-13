Include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.8.1 # or a later release
)
FetchContent_MakeAvailable(Catch2)

set(PERF_CPP_TEST
        test/access_benchmark.cpp
        test/counter_definition.cpp
        test/metric.cpp
        test/event_counter.cpp
        test/sampler.cpp
        test/hardware_info.cpp
)

add_executable(tests ${PERF_CPP_TEST})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain perf-cpp)
target_include_directories(tests PRIVATE Catch2::Catch2WithMain test)
set_target_properties(tests
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)