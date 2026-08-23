from conan import ConanFile
from conan.errors import ConanInvalidConfiguration
from conan.tools.build import check_min_cppstd
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, rmdir
from conan.tools.scm import Version
import os

required_conan_version = ">=2.0.9"


class PerfCppConan(ConanFile):
    name = "perf-cpp"
    version = "1.1.0"
    license = "Apache-2.0"
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

    package_type = "library"
    settings = "os", "arch", "compiler", "build_type"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    implements = ["auto_shared_fpic"]

    exports_sources = "CMakeLists.txt", "src/*", "include/*", "cmake/*", "LICENSE", ".clang-tidy"

    @property
    def _min_cppstd(self):
        return 17

    @property
    def _compilers_minimum_version(self):
        # CMakeLists.txt enforces the same bound: perf-cpp needs complete std::from_chars support.
        return {"gcc": "11", "clang": "14"}

    def validate(self):
        if self.settings.os != "Linux":
            raise ConanInvalidConfiguration(
                f"{self.ref} is only supported on Linux: it is built on the perf_event_open syscall."
            )
        check_min_cppstd(self, self._min_cppstd)
        minimum_version = self._compilers_minimum_version.get(str(self.settings.compiler))
        if minimum_version and Version(self.settings.compiler.version) < minimum_version:
            raise ConanInvalidConfiguration(
                f"{self.ref} requires {self.settings.compiler} >= {minimum_version}."
            )

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        # Upstream uses a custom option name rather than BUILD_SHARED_LIBS.
        tc.cache_variables["BUILD_LIB_SHARED"] = bool(self.options.shared)
        tc.cache_variables["BUILD_EXAMPLES"] = False
        tc.cache_variables["BUILD_TESTS"] = False
        # Would shell out to python3 to generate a processor-specific event table at build time.
        tc.cache_variables["GEN_PROCESSOR_EVENTS"] = False
        # Defaults to ON for a standalone build, which this is; packaged builds should not lint.
        tc.cache_variables["ENABLE_CLANG_TIDY"] = False
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        copy(self, "LICENSE", self.source_folder, os.path.join(self.package_folder, "licenses"))
        cmake = CMake(self)
        cmake.install()
        # Conan generates its own CMake config files from package_info() below.
        rmdir(self, os.path.join(self.package_folder, "lib", "cmake"))

    def package_info(self):
        self.cpp_info.libs = ["perf-cpp"]
        self.cpp_info.set_property("cmake_file_name", "perf-cpp")
        self.cpp_info.set_property("cmake_target_name", "perf-cpp::perf-cpp")
        # The sampler's overflow worker runs on a std::thread.
        self.cpp_info.system_libs.append("pthread")
