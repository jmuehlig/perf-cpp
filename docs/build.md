# How to build and include *perf-cpp* in your project

## By hand
* Download the source code (`git clone https://github.com/jmuehlig/perf-cpp.git`)
* Within the cloned directory, call `cmake`, optionally define build options, desired targets or an installation prefix:
  ```
  cmake -S . -B build -DBUILD_EXAMPLES=ON -DCMAKE_INSTALL_PREFIX=/path/to/libperf-cpp
  cmake --build build -t perf-cpp -t perf-list -t examples
  cmake --install build # to installl the library
  ```
* Afterwards, the library should be available for discovery with `find_package` (see below).

## Using `CMake` and `FetchContent`
* Add `include(FetchContent)` to your `CMakeLists.txt`
* Define an external project:
```
include(FetchContent)
FetchContent_Declare(
  perf-cpp-external
  GIT_REPOSITORY "https://github.com/jmuehlig/perf-cpp"
  GIT_TAG "v0.7.0"
)
FetchContent_MakeAvailable(perf-cpp-external)
```
* Add `-DBUILD_SHARED_LIBS=ON` if you want to build a shared library instead of static
* Add `perf-cpp` to your linked libraries
* Add `${perf-cpp-external_SOURCE_DIR}/include/` to your include directories

## Using `CMake` and `find_package`

This assumes `perf-cpp` is already installed on your system. Then, it should be enough to call `find_package` and link it against your target:

```
find_package(perf-cpp REQUIRED)
target_link_libraries(perf-cpp::perf-cpp)
```

## Build Examples

Configure the library with `-DBUILD_EXAMPLES=1` and build the `examples` target
```
cmake -S . -B build -DBUILD_EXAMPLES=1
cmake --build build --target examples
```

---

## Notes for older Linux Kernels
###  Linux Kernel version `< 5.13`
The counter `cgroup-switches` is only provided since Kernel `5.13`.
If you have an older Kernel, the counter cannot be used and will be deactivated.

### Linux Kernel version `< 5.12`
Sampling *weight as struct* (`Type::WeightStruct`, see [sampling documentation](sampling.md)) is only provided since Kernel `5.12`.
However, you can sample for weight using `Type::Weight`. To avoid compilation errors, you have to define


    -DNO_PERF_SAMPLE_WEIGHT_STRUCT


when compiling the binary that is linked against `libperf-cpp`.

### Linux Kernel version `< 5.11`
Sampling *data page size* and *code page size*  (see [sampling documentation](sampling.md)) is only provided since Kernel `5.11`.
If you have an older Kernel you need to define


    -DNO_PERF_SAMPLE_DATA_PAGE_SIZE -DNO_PERF_SAMPLE_CODE_PAGE_SIZE


when compiling the binary that is linked against `libperf-cpp`.
