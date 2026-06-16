# Conan Recipe: build & local-cache publish for perf-cpp

**Date:** 2026-06-16
**Scope:** Fix the existing `conanfile.py`, install Conan locally, build and cache the package, verify with `test_package/`. No remote publish in this phase.

---

## Goal

Get `perf-cpp/1.0.0` installable via `conan install perf-cpp/1.0.0` from the local Conan cache. The recipe already exists (`conanfile.py` + `test_package/`) but has never been built or tested, and contains two bugs that will prevent a successful build.

---

## Conan installation & profile

Install Conan 2.x via `pipx`:

```
pipx install conan
conan profile detect
```

`pipx` isolates Conan in its own virtual environment — no system Python pollution, no `sudo`. `conan profile detect` auto-generates a default profile from the current compiler. The recipe's `validate()` method enforces Linux-only, so the profile only needs GCC 11+ or Clang 14+ (already satisfied by the dev machine).

---

## conanfile.py fixes

### Bug 1 — Missing `cmake/` in `exports_sources` (critical)

`CMakeLists.txt` calls:

```cmake
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/perf-cppConfig.cmake.in" ...)
```

Without `cmake/*` in `exports_sources`, Conan will not copy the template into the build sandbox and the package build will fail with a CMake file-not-found error.

**Fix:** Add `"cmake/*"` to the tuple:

```python
exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "LICENSE", ".clang-tidy"
```

### Bug 2 — fPIC cleanup in wrong hook (style/correctness)

In Conan 2.x:
- `config_options()` — removes options that are unconditionally unavailable (e.g., no `fPIC` on Windows).
- `configure()` — makes option-based decisions after values are resolved.

The current code puts the `shared → remove fPIC` logic in `config_options()`. The correct placement is `configure()`.

**Fix:**

```python
def config_options(self):
    pass  # nothing to remove unconditionally on Linux

def configure(self):
    if self.options.shared:
        self.options.rm_safe("fPIC")
```

### Retained workaround

`package()` manually copies `*.a` / `*.so*` from the build folder in addition to calling `cmake.install()`. This is kept as a safety net in case `cmake.install()` doesn't place the library under `{package_folder}/lib/`. It is harmless if redundant.

---

## Build & test workflow

```bash
# 1. Build and cache the package
conan create . --build=missing

# 2. Verify the consumer
conan test test_package perf-cpp/1.0.0
```

`conan create` exports sources, builds the library against the default profile, and installs the result in the local cache as `perf-cpp/1.0.0`.

`conan test` builds `test_package/` against the cached package and runs the binary. The existing `test_package/src/example.cpp` instantiates `perf::CounterDefinition` and `perf::EventCounter` — enough to confirm headers, linkage, and the `perf-cpp::perf-cpp` CMake target all resolve correctly. No changes to `test_package/` are needed.

---

## Out of scope

- Remote publishing (ConanCenter, Artifactory) — deferred.
- CI integration — deferred.
- Extending `test_package/` beyond the current minimal smoke test — not required at this stage.
