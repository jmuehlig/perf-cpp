#include <catch2/catch_test_macros.hpp>
#include <perfcpp/hardware_info.h>

TEST_CASE("number of performance counters", "[HardwareInfo]")
{
  REQUIRE(perf::HardwareInfo::performance_counters_per_logical_core() > 0U);
}