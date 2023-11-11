cmake_minimum_required(VERSION 3.10)
project(libperf-cpp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE RELEASE)

include_directories(include/)

add_library(perf-cpp src/counter.cpp src/counter_definition.cpp src/perf.cpp)
