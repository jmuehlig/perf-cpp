Include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.4.0 # or a later release
)
FetchContent_MakeAvailable(Catch2)

set(PERF_CPP_TEST
        test/counter_definition.cpp
        test/metric.cpp
        test/event_counter.cpp
)

add_executable(tests ${PERF_CPP_TEST})
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain perf-cpp)
target_include_directories(tests PRIVATE Catch2::Catch2WithMain)
set_target_properties(tests
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)