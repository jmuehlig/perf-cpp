#include <perfcpp/config.h>
#include <perfcpp/hardware_info.h>

perf::Process perf::Process::Any = perf::Process{ -1 };
perf::Process perf::Process::Calling = perf::Process{ 0 };
perf::CpuCore perf::CpuCore::Any = perf::CpuCore{ -1 };

perf::Config::Config() noexcept
{
  if (const auto physical_performance_counters = HardwareInfo::physical_performance_counters_per_logical_core();
      physical_performance_counters > 0U) {
    this->_max_groups = physical_performance_counters;
    this->_max_counters_per_group = HardwareInfo::events_per_physical_performance_counter();
  }
}