# Building and Project Integration

## Building Manually

```bash
git clone https://github.com/jmuehlig/perf-cpp.git
cd perf-cpp
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

The library will then be available via `find_package` (see [below](#via-find_package)).

## Including into CMake Projects

### Via FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.13.0"
)
FetchContent_MakeAvailable(perf-cpp-external)
```

Then link against `perf-cpp` and add `${perf-cpp-external_SOURCE_DIR}/include/` to your include directories.

### Via ExternalProject

```cmake
include(ExternalProject)
ExternalProject_Add(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.13.0"
  PREFIX "lib/perf-cpp"
  INSTALL_COMMAND cmake -E echo ""
)
```

Then add `lib/perf-cpp/src/perf-cpp-external/include` to your include directories and `lib/perf-cpp/src/perf-cpp-external-build` to your link directories.

### Via find_package

If *perf-cpp* is [installed](#installing) on your system:

```cmake
find_package(perf-cpp REQUIRED)
target_link_libraries(your_target perf-cpp::perf-cpp)
```
