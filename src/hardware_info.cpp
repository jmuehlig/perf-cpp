#include <algorithm>
#include <filesystem>
#include <perfcpp/hardware_info.h>
#include <unistd.h>

bool
perf::HardwareInfo::is_intel_aux_counter_required()
{
  if (HardwareInfo::is_intel()) {
    return std::filesystem::exists(std::filesystem::path("/sys/bus/event_source/devices/cpu/events/mem-loads-aux")) ||
           std::filesystem::exists(
             std::filesystem::path("/sys/bus/event_source/devices/cpu_core/events/mem-loads-aux"));
  }

  return false;
}

std::uint64_t
perf::HardwareInfo::memory_page_size()
{
  return std::uint64_t(std::max(0L, ::sysconf(_SC_PAGESIZE)));
}
