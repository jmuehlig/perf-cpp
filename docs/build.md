# How to build and include *perf-cpp* in your project

## By hand
* Download the source code (`git clone https://github.com/jmuehlig/perf-cpp.git`)
* Within the directory, call `cmake .` and `make` within the downloaded folder
* Copy the `include/` directory and the static library `libperf-cpp.a` to your project
* Include the `include/` folder and link the library: `-lperf-cpp`

## Using `CMake` and `ExternalProject`
* Add `include(ExternalProject)` to your `CMakeLists.txt`
* Define an external project:
```
ExternalProject_Add(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.2.1"
  PREFIX "path/to/your/libs/perf-cpp"
  INSTALL_COMMAND cmake -E echo ""
)
```
* Add `path/to/your/libs/perf-cpp/src/perf-cpp-external/include` to your `include_directories()`
* Add `perf-cpp` to your linked libraries