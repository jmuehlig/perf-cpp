#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <perfcpp/sample/ibs_decoder.hpp>
#include <vector>

namespace {
/// Builds a zero-filled raw IBS buffer of the given total size (including the 4-byte caps word).
std::vector<std::byte>
make_raw(const std::size_t total_size)
{
  return std::vector<std::byte>(total_size, std::byte{ 0 });
}
}

TEST_CASE("IBSFetchDecoder - 28-byte buffer has only three base MSRs", "[ibs][fetch]")
{
  /// 4 caps + 3 * 8 = 28 bytes; extended MSR not present.
  auto raw = make_raw(28U);
  const auto decoder = perf::IBSFetchDecoder{ raw };

  CHECK(decoder.is_base_data_complete() == true);
  CHECK(decoder.has_extended_fetch_control() == false);
  CHECK(decoder.itlb_refill_latency() == std::nullopt);
}

TEST_CASE("IBSFetchDecoder - 36-byte buffer includes extended MSR with known latency", "[ibs][fetch]")
{
  /// 4 caps + 4 * 8 = 36 bytes; extended MSR present at byte 28.
  auto raw = make_raw(36U);
  const std::uint16_t expected = 42U;
  std::memcpy(raw.data() + 28U, &expected, sizeof(expected));

  const auto decoder = perf::IBSFetchDecoder{ raw };

  CHECK(decoder.is_base_data_complete() == true);
  CHECK(decoder.has_extended_fetch_control() == true);
  REQUIRE(decoder.itlb_refill_latency().has_value());
  CHECK(decoder.itlb_refill_latency().value() == expected);
}

TEST_CASE("IBSFetchDecoder - truncated buffer marks base data incomplete", "[ibs][fetch]")
{
  /// Only a few bytes — base MSRs are not fully present.
  auto raw = make_raw(10U);
  const auto decoder = perf::IBSFetchDecoder{ raw };

  CHECK(decoder.is_base_data_complete() == false);
  CHECK(decoder.has_extended_fetch_control() == false);
  CHECK(decoder.itlb_refill_latency() == std::nullopt);
}

TEST_CASE("IBSOpDecoder - 60-byte buffer has seven base MSRs, no branch target", "[ibs][op]")
{
  /// 4 caps + 7 * 8 = 60 bytes; branch target MSR not present.
  auto raw = make_raw(60U);
  const auto decoder = perf::IBSOpDecoder{ raw };

  CHECK(decoder.is_base_data_complete() == true);
  CHECK(decoder.has_branch_target_address() == false);
  CHECK(decoder.branch_target_address() == std::nullopt);
}

TEST_CASE("IBSOpDecoder - 68-byte buffer includes branch target with known address", "[ibs][op]")
{
  /// 4 caps + 8 * 8 = 68 bytes; branch target MSR present at byte 60.
  auto raw = make_raw(68U);
  const std::uintptr_t expected = 0xDEADBEEFUL;
  std::memcpy(raw.data() + 60U, &expected, sizeof(expected));

  const auto decoder = perf::IBSOpDecoder{ raw };

  CHECK(decoder.is_base_data_complete() == true);
  CHECK(decoder.has_branch_target_address() == true);
  REQUIRE(decoder.branch_target_address().has_value());
  CHECK(decoder.branch_target_address().value() == expected);
}

TEST_CASE("IBSOpDecoder - truncated buffer marks base data incomplete", "[ibs][op]")
{
  auto raw = make_raw(10U);
  const auto decoder = perf::IBSOpDecoder{ raw };

  CHECK(decoder.is_base_data_complete() == false);
  CHECK(decoder.has_branch_target_address() == false);
  CHECK(decoder.branch_target_address() == std::nullopt);
}
