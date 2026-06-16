# Conan Recipe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix the existing `conanfile.py`, install Conan, build `perf-cpp/1.0.0` into the local Conan cache, and verify it with `test_package/`.

**Architecture:** Two bugs in the existing recipe are fixed first (missing `cmake/` export, wrong hook for fPIC). Then Conan is installed via `pipx`, a default profile is detected, and `conan create` + `conan test` are run to confirm the full pipeline works.

**Tech Stack:** Conan 2.x (Python packaging), CMake 3.10+, GCC 11+ or Clang 14+

---

## File Map

| File | Action | What changes |
|---|---|---|
| `conanfile.py` | Modify | Add `cmake/*` to `exports_sources`; replace `config_options()` with `configure()` |

No other files change. `test_package/` is already correct.

---

### Task 1: Install Conan via pipx

**Files:** none

- [ ] **Step 1: Install pipx**

```bash
sudo apt-get install -y pipx
```

Expected output includes: `Setting up pipx`

- [ ] **Step 2: Ensure pipx binaries are on PATH**

```bash
pipx ensurepath
```

Then open a new terminal (or `source ~/.bashrc` / `source ~/.zshrc`) so the path takes effect.

- [ ] **Step 3: Install Conan**

```bash
pipx install conan
```

Expected: `installed package conan ...`

- [ ] **Step 4: Verify**

```bash
conan --version
```

Expected: `Conan version 2.x.x`

---

### Task 2: Create a default Conan profile

**Files:** none (writes to `~/.conan2/profiles/default`)

- [ ] **Step 1: Auto-detect compiler**

```bash
conan profile detect
```

Conan inspects the current compiler and generates `~/.conan2/profiles/default`. Expected output ends with something like:

```
Profile 'default' created
```

- [ ] **Step 2: Verify the profile meets minimum requirements**

```bash
conan profile show
```

Check that `compiler` is `gcc` with version ≥ 11, or `clang` with version ≥ 14. The `compiler.cppstd` line should show `gnu17` or `17`. If `cppstd` is missing or lower, set it:

```bash
conan profile update settings.compiler.cppstd=gnu17 default
```

---

### Task 3: Fix `conanfile.py`

**Files:**
- Modify: `conanfile.py`

The recipe has two bugs. Fix both in one edit.

- [ ] **Step 1: Apply the fixes**

Open `conanfile.py`. Make these two changes:

**Change 1** — add `"cmake/*"` to `exports_sources` (line 40):

```python
# Before:
exports_sources = "CMakeLists.txt", "src/*", "include/*", "LICENSE", ".clang-tidy"

# After:
exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "LICENSE", ".clang-tidy"
```

**Change 2** — replace `config_options()` with `configure()` (lines 48–49):

```python
# Before:
def config_options(self):
    if self.options.shared:
        self.options.rm_safe("fPIC")

# After:
def configure(self):
    if self.options.shared:
        self.options.rm_safe("fPIC")
```

The full file after both changes:

```python
from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy
import os


class PerfCppConan(ConanFile):
    name = "perf-cpp"
    version = "1.0.0"
    license = "LGPL-3.0-only"
    author = "Jan Muehlig"
    url = "https://github.com/jmuehlig/perf-cpp"
    homepage = "https://github.com/jmuehlig/perf-cpp"
    description = (
        "Lightweight C++17 library for hardware performance counter "
        "monitoring and sampling using the Linux perf subsystem."
    )
    topics = (
        "performance",
        "profiling",
        "perf",
        "hardware-counters",
        "sampling",
        "pebs",
        "ibs",
        "linux",
    )

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "LICENSE", ".clang-tidy"

    def validate(self):
        if self.settings.os != "Linux":
            raise ConanInvalidConfiguration(
                "perf-cpp requires Linux (uses perf_event_open syscall)."
            )

    def configure(self):
        if self.options.shared:
            self.options.rm_safe("fPIC")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        tc.variables["BUILD_LIB_SHARED"] = self.options.shared
        tc.variables["BUILD_EXAMPLES"] = False
        tc.variables["BUILD_TESTS"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", src=self.source_folder, dst=os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        ## cmake install may not land the library under {package_folder}/lib when
        ## PERF_CPP_INSTALL_CMAKEDIR is cached as an absolute path; copy explicitly as fallback.
        copy(self, "*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)
        copy(self, "*.so*", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"), keep_path=False)

    def package_info(self):
        self.cpp_info.libs = ["perf-cpp"]
        self.cpp_info.set_property("cmake_file_name", "perf-cpp")
        self.cpp_info.set_property("cmake_target_name", "perf-cpp::perf-cpp")
```

- [ ] **Step 2: Commit**

```bash
git add conanfile.py
git commit -m "fix: add cmake/ to conan exports_sources; move fPIC to configure()"
```

---

### Task 4: Build and cache the package

**Files:** none (output goes into `~/.conan2/p/`)

- [ ] **Step 1: Run `conan create`**

From the project root:

```bash
conan create . --build=missing
```

This exports the sources, invokes CMake to build the library, runs `cmake install`, and stores the result in the local Conan cache as `perf-cpp/1.0.0`. The build can take 1–2 minutes.

Expected last lines:

```
perf-cpp/1.0.0: Package ... created
perf-cpp/1.0.0: Full package reference: perf-cpp/1.0.0#...
```

If the build fails with `cmake/perf-cppConfig.cmake.in: No such file`, Task 3 Step 1 was not applied — re-check `exports_sources`.

If the build fails with a compiler warning treated as error, this is likely a `-Wconversion` hit from a system header. Check the error message; the workaround is to pass `-DCMAKE_CXX_FLAGS=-Wno-conversion` via `tc.variables` in `generate()`.

- [ ] **Step 2: Confirm the package is in the cache**

```bash
conan list perf-cpp/1.0.0
```

Expected:

```
Local Cache
  perf-cpp
    perf-cpp/1.0.0
      ...
```

---

### Task 5: Verify with test_package

**Files:** none

- [ ] **Step 1: Run `conan test`**

```bash
conan test test_package perf-cpp/1.0.0
```

This builds `test_package/` against the cached package and runs `test_perf_cpp`.

Expected output (last lines):

```
perf-cpp: Successfully configured 'instructions' counter.
...
perf-cpp/1.0.0 (test package): PASSED
```

- [ ] **Step 2: Confirm success**

The binary must print `perf-cpp: Successfully configured 'instructions' counter.` and exit 0. Any linker error means the `.a` file was not correctly placed in `{package_folder}/lib/` — re-examine the `package()` step output from Task 4 and check whether the library was copied.
