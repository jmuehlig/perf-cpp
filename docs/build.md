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
---

## Building by Hand
### Build the Library
#### 1) Download the source code

```
git clone git clone https://github.com/jmuehlig/perf-cpp.git
cd perf-cpp
git checkout v0.7.1   # optional
```

#### 2) Generate the Makefile and build

```
cmake . -B build 
cmake --build build
```

**Note** that the build directory `build` can be replaced by any directory you want (including `.`).

### Install the Library
To install the library, you need to define the `CMAKE_INSTALL_PREFIX`:

```
cmake . -B build -DCMAKE_INSTALL_PREFIX=/path/to/install/dir
cmake --build build
```

Afterward you can install the library by:
```
cmake --install build
```

The library should be available for discovery with CMake and `find_package` (see [below](#cmake-and-find_package)).

**Note** that the build directory `build` can be replaced by any directory you want (including `.`).

### Build Examples
Configure the library with `-DBUILD_EXAMPLES=1` and build the `examples` target
```
cmake . -B build -DBUILD_EXAMPLES=1
cmake --build build --target examples
```

The example binaries can be found in `build/examples/bin`.

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
