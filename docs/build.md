# Building and Project Integration

## Building Manually

```bash
git clone https://github.com/jmuehlig/perf-cpp.git
cd perf-cpp
git checkout v1.1
cmake . -B build
cmake --build build
```

### CMake Options

| Option | Default | Description |
|---|---|---|
| `-DBUILD_EXAMPLES=ON` | `OFF` | Build example binaries into `build/bin` |
| `-DBUILD_LIB_SHARED=ON` | `OFF` | Build as shared library instead of static |
| `-DBUILD_TESTS=ON` | `OFF` | Build unit tests |
| `-DGEN_PROCESSOR_EVENTS=ON` | `OFF` | Embed processor-specific events at compile time (see [customizing events](counters.md)) |

Example with multiple options:
```bash
cmake . -B build -DBUILD_EXAMPLES=ON -DGEN_PROCESSOR_EVENTS=ON
cmake --build build
```

> [!NOTE]
> `-DGEN_PROCESSOR_EVENTS=ON` reads events from the [event library](./counters.md#loading-from-the-event-library) and generates a source file that can grow large, increasing compilation time significantly.

### Installing

```bash
cmake . -B build -DCMAKE_INSTALL_PREFIX=/path/to/install/dir
cmake --build build
cmake --install build
```

This installs the library into `lib/`, the headers into `include/`, and the CMake package config files into `lib/cmake/perf-cpp/`, relative to the install prefix.
The library will then be available via `find_package` (see [below](#via-find_package)).
If you install to a non-standard prefix, point CMake to it when configuring the consuming project, e.g., via `-DCMAKE_PREFIX_PATH=/path/to/install/dir`.

## Including into CMake Projects

### Via FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v1.1"
)
FetchContent_MakeAvailable(perf-cpp-external)
```

Then link against `perf-cpp`; the include directory and the C++17 requirement propagate automatically:

```cmake
target_link_libraries(your_target perf-cpp)
```

### Via ExternalProject

```cmake
include(ExternalProject)
ExternalProject_Add(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v1.1"
  PREFIX "lib/perf-cpp"
  INSTALL_COMMAND cmake -E echo ""
)
```

Then retrieve the build paths and link against `perf-cpp`:

```cmake
ExternalProject_Get_Property(perf-cpp-external SOURCE_DIR BINARY_DIR)
include_directories(${SOURCE_DIR}/include)
link_directories(${BINARY_DIR})
target_link_libraries(your_target perf-cpp)
add_dependencies(your_target perf-cpp-external)
```

### Via Conan

Add *perf-cpp* to your `conanfile.txt`:

```ini
[requires]
perf-cpp/1.1.0

[generators]
CMakeDeps
CMakeToolchain
```

Then install dependencies and configure:

```bash
conan install . --build=missing
cmake . -B build --preset conan-release
cmake --build build
```

Link in your `CMakeLists.txt`:

```cmake
find_package(perf-cpp REQUIRED CONFIG)
target_link_libraries(your_target perf-cpp::perf-cpp)
```

### Via find_package

If *perf-cpp* is [installed](#installing) on your system:

```cmake
find_package(perf-cpp 1.1 REQUIRED)
target_link_libraries(your_target perf-cpp::perf-cpp)
```

The version argument is optional; the package accepts any request with the same major version (e.g., `find_package(perf-cpp 1.1)` matches an installed `1.2.0`, but not `2.0.0`).
