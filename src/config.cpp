#include <perfcpp/config.h>

perf::Process perf::Process::Any = perf::Process{ -1 };
perf::Process perf::Process::Calling = perf::Process{ 0 };
perf::CpuCore perf::CpuCore::Any = perf::CpuCore{ -1 };