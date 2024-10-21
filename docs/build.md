# How to build and include *perf-cpp* in your project

## Table of Contents
- [Building by Hand](#building-by-hand)
  - [Build](#build-the-library)
  - [Install](#install-the-library)
  - [Build Examples](#build-examples)
- [Including into `CMakeLists.txt`](#including-into-cmakeliststxt)
  - [ExternalProject](#cmake-and-externalproject)
  - [FetchContent](#cmake-and-fetchcontent)
  - [find_package](#cmake-and-find_package)
- [Notes for older Linux Kernels](#notes-for-older-linux-kernels)
  - [Linux Kernel version `< 5.13`](#linux-kernel-version--513)
  - [Linux Kernel version `< 5.12`](#linux-kernel-version--512)
  - [Linux Kernel version `< 5.11`](#linux-kernel-version--511)
---

## Building by Hand
### Build the Library
#### Download the source code

```
git clone git clone https://github.com/jmuehlig/perf-cpp.git
cd perf-cpp
git checkout v0.7.1   # optional
```

#### Generate the Makefile and build

```
cmake . -B build --DCMAKE_INSTALL_PREFIX=/path/to/libperf-cpp
cmake --build build
```

**Note** that the build directory `build` can be replaced by any directory you want (including `.`).

### Install the Library
After building, you can install the library by:

```
cmake --install build
```

Afterward, the library should be available for discovery with CMake and `find_package` (see [below](#cmake-and-find_package)).

Note that the build directory `build` can be replaced by any directory you want (including `.`).

### Build Examples
Configure the library with `-DBUILD_EXAMPLES=1` and build the `examples` target
```
cmake . -B build -DBUILD_EXAMPLES=1
cmake --build build --target examples
```

## Including into `CMakeLists.txt`
*perf-cpp*  uses [CMake](https://cmake.org/) as a build system, allowing for including *perf-cpp* into further CMake projects.
You can choose one of the following approaches.

### CMake and ExternalProject
* Add `include(ExternalProject)` to your `CMakeLists.txt`
* Define an external project:
```
ExternalProject_Add(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.7.1"
  PREFIX "lib/perf-cpp"
  INSTALL_COMMAND cmake -E echo ""
)
```
* Add `lib/perf-cpp/src/perf-cpp-external/include` to your `include_directories()`
* Add `lib/perf-cpp/src/perf-cpp-external-build` to your `link_directories()`

Note that **lib/** can be replaced by any folder you want to store the library in.
  

### CMake and FetchContent
* Add `include(FetchContent)` to your `CMakeLists.txt`
* Define an external project:
```
include(FetchContent)
FetchContent_Declare(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.7.1"
)
FetchContent_MakeAvailable(perf-cpp-external)
```
* Add `-DBUILD_SHARED_LIBS=ON` if you want to build a shared library instead of static
* Add `perf-cpp` to your linked libraries
* Add `${perf-cpp-external_SOURCE_DIR}/include/` to your include directories

### CMake and find_package

This assumes `perf-cpp` is already installed on your system. Then, it should be enough to call `find_package` and link it against your target:

```
find_package(perf-cpp REQUIRED)
target_link_libraries(perf-cpp::perf-cpp)
```
---

## Notes for older Linux Kernels
###  Linux Kernel version `< 5.13`
The counter `cgroup-switches` is only provided since Kernel `5.13`.
If you have an older Kernel, the counter cannot be used and will be deactivated.

### Linux Kernel version `< 5.12`
Sampling *weight as struct* ( see [sampling documentation](sampling.md)) is only provided since Kernel `5.12`.
However, you can sample for weight using normal weight. To avoid compilation errors, you have to define


    -DNO_PERF_SAMPLE_WEIGHT_STRUCT


when compiling the binary that is linked against `libperf-cpp`.

### Linux Kernel version `< 5.11`
Sampling *data page size* and *code page size*  (see [sampling documentation](sampling.md)) is only provided since Kernel `5.11`.
If you have an older Kernel you need to define


    -DNO_PERF_SAMPLE_DATA_PAGE_SIZE -DNO_PERF_SAMPLE_CODE_PAGE_SIZE


when compiling the binary that is linked against `libperf-cpp`.
