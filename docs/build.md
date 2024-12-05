# How to build and include *perf-cpp* in your project
*perf-cpp* can be build manually or included into CMake projects.

## Table of Contents
- [Building Manually](#building-manually)
  - [Build the Library](#build-the-library)
  - [Install the Library](#install-the-library)
  - [Build the Examples](#build-examples)
  - [Building as a Dynamically Linked Library](#building-as-a-dynamically-linked-library)
- [Use CMake](#including-into-cmakeliststxt)
  - [ExternalProject](#via-externalproject)
  - [FetchContent](#via-fetchcontent)
  - [find_package](#via-find_package)
---

## Building Manually
**Note**: Throughout the documentation, we use `./build` as the build directory.
However, the build directory can be any directory of your choice (including `.`).

### Build the Library
#### Download the source code

```bash
git clone https://github.com/jmuehlig/perf-cpp.git
cd perf-cpp

# Optional: switch to latest stable version
git checkout v0.9.0   
```

#### Generate the Makefile and Build

```bash
cmake . -B build 
cmake --build build
```

### Install the Library
To install the library, specify the `CMAKE_INSTALL_PREFIX`:
```bash
# Generate Makefile
cmake . -B build -DCMAKE_INSTALL_PREFIX=/path/to/install/dir

# Build
cmake --build build

# Install
cmake --install build
```

The library will then be available for discovery via CMake and `find_package` (see [below](#via-find_package)).

### Build Examples
Enable example compilation with `-DBUILD_EXAMPLES=1` and build the `examples` target:

```bash
# Generate Makefile
cmake . -B build -DBUILD_EXAMPLES=1

# Build Library and Examples
cmake --build build --target examples
```

The example binaries will be located in `build/examples/bin`.

### Building as a Dynamically Linked Library
By default, *perf-cpp* is build as a **static** library.
You can request to build a **shared** library with `-DBUILD_LIB_SHARED=1`:

```bash
cmake . -B build -DBUILD_LIB_SHARED=1
cmake --build build
```

## Including into `CMakeLists.txt`
*perf-cpp* uses [CMake](https://cmake.org/) as its build system, facilitating integration into additional CMake projects. 
Choose from the following methods:

### Via ExternalProject
Include `ExternalProject` in your `CMakeLists.txt` and define the project:

```cmake
include(ExternalProject)
ExternalProject_Add(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.9.0"
  PREFIX "lib/perf-cpp"
  INSTALL_COMMAND cmake -E echo ""
)
```
* Add `lib/perf-cpp/src/perf-cpp-external/include` to your `include_directories()`.
* Add `lib/perf-cpp/src/perf-cpp-external-build` to your `link_directories()`.

Note: The directory `lib/` can be any folder of your choice.

### Via FetchContent
Include `FetchContent` in your `CMakeLists.txt` and define the project:

```cmake
include(FetchContent)
FetchContent_Declare(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.9.0"
)
FetchContent_MakeAvailable(perf-cpp-external)
```
* Add `perf-cpp` to your linked libraries.
* Add `${perf-cpp-external_SOURCE_DIR}/include/` to your include directories.

### Via find_package
If *perf-cpp* is already installed on your system (see [install instructions above](#install-the-library)), you can simply use `find_package` to link it with your project:

```cmake
find_package(perf-cpp REQUIRED)
target_link_libraries(perf-cpp::perf-cpp)
```
