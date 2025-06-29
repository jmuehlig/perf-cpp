#include <catch2/catch_test_macros.hpp>
#include <perfcpp/hardware_info.h>

TEST_CASE("number of performance counters", "[HardwareInfo]")
{
  REQUIRE(perf::HardwareInfo::physical_performance_counters_per_logical_core() > 1U);
}

TEST_CASE("number of events per performance counter", "[HardwareInfo]")
{
  REQUIRE(perf::HardwareInfo::events_per_physical_performance_counter() > 1U);
}