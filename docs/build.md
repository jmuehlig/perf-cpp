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
  GIT_TAG "v0.5.0"
  PREFIX "path/to/your/libs/perf-cpp"
  INSTALL_COMMAND cmake -E echo ""
)
```
* Add `path/to/your/libs/perf-cpp/src/perf-cpp-external/include` to your `include_directories()`
* Add `perf-cpp` to your linked libraries

---

## Notes for older Linux Kernels
###  Linux Kernel version `< 5.13`
The counter `cgroup-switches` is only provided since Kernel `5.13`.
If you have an older Kernel, the counter cannot be used and will be deactivated.

### Linux Kernel version `< 5.12`
Sampling *weight as struct* (`Type::WeightStruct`, see [sampling documentation](docs/sampling.md)) is only provided since Kernel `5.12`.
However, you can sample for weight using `Type::Weight`. To avoid compilation errors, you have to define 


    -DNO_PERF_SAMPLE_WEIGHT_STRUCT


when compiling the binary that is linked against `libperf-cpp`.

### Linux Kernel version `< 5.11`
Sampling *data page size* and *code page size*  (see [sampling documentation](docs/sampling.md)) is only provided since Kernel `5.11`.
If you have an older Kernel you need to define


    -DNO_PERF_SAMPLE_DATA_PAGE_SIZE -DNO_PERF_SAMPLE_CODE_PAGE_SIZE


when compiling the binary that is linked against `libperf-cpp`.

