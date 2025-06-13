#include <perfcpp/config.h>

perf::Process perf::Process::ANY = perf::Process{-1};
perf::Process perf::Process::CALLING = perf::Process{0};
perf::CpuCore perf::CpuCore::ANY = perf::CpuCore{-1};