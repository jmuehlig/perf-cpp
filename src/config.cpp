#include <perfcpp/counter/config.hpp>
#include <perfcpp/exception.hpp>
#include <perfcpp/hardware_info.hpp>
#include <fcntl.h>

perf::Process perf::Process::Any = Process{ -1 };
perf::Process perf::Process::Calling = Process{ 0 };
perf::CpuCore perf::CpuCore::Any = CpuCore{};

perf::CGroupMonitor::CGroupMonitor(const std::filesystem::path& path)
{
  const auto raw_fd = ::open(path.c_str(), O_RDONLY);
  if (raw_fd < 0) {
    throw CannotOpenCGroupError{ path.string(), errno };
  }
  this->_file_descriptor = util::SharedFileDescriptor{ raw_fd };
}

perf::CGroupMonitor::CGroupMonitor(const std::string& name)
  : CGroupMonitor(std::filesystem::path{ "/sys/fs/cgroup/" + name })
{
}

perf::Config::Config() noexcept
{
  /// Try to read the number of generic performance counters from the hardware (either from cpuid or by trying).
  if (const auto generic_counters = HardwareInfo::physical_generic_performance_counters_per_logical_core();
      generic_counters > 0U) {
    /// If that worked, also read the number of events per physical performance counter.
    this->_num_physical_counters = generic_counters;
    this->_num_events_per_physical_counter = HardwareInfo::events_per_physical_performance_counter();
  }
}