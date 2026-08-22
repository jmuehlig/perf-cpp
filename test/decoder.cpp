#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <limits>
#include <linux/perf_event.h>
#include <perfcpp/counter/group.hpp>
#include <perfcpp/counter/requested_event.hpp>
#include <perfcpp/counter_definition.hpp>
#include <perfcpp/sample/decoder.hpp>
#include <perfcpp/sample/recording_values.hpp>
#include <vector>

namespace {
/// Builds a physically over-sized, zero-padded buffer holding one perf_event_header (from the
/// kernel's own <linux/perf_event.h>, so its layout is authoritative) followed by the given payload.
/// The extra padding beyond the declared record size guarantees that even a decoder which reads past
/// what it "should" can never touch unallocated memory — these tests are about decoding behavior, not
/// about probing memory safety.
std::vector<std::byte>
make_record(const std::uint32_t type, const std::uint16_t misc, const std::vector<std::byte>& payload)
{
  auto header = perf_event_header{};
  header.type = type;
  header.misc = misc;
  header.size = static_cast<std::uint16_t>(sizeof(header) + payload.size());

  constexpr auto physical_padding = std::size_t{ 64U };
  auto buffer = std::vector<std::byte>(sizeof(header) + payload.size() + physical_padding, std::byte{ 0 });
  std::memcpy(buffer.data(), &header, sizeof(header));
  if (!payload.empty()) {
    std::memcpy(buffer.data() + sizeof(header), payload.data(), payload.size());
  }

  return buffer;
}

std::vector<std::byte>
u64_payload(const std::uint64_t value)
{
  auto bytes = std::vector<std::byte>(sizeof(value));
  std::memcpy(bytes.data(), &value, sizeof(value));
  return bytes;
}

std::vector<std::byte>
concat(const std::initializer_list<std::vector<std::byte>> parts)
{
  auto result = std::vector<std::byte>{};
  for (const auto& part : parts) {
    result.insert(result.end(), part.begin(), part.end());
  }
  return result;
}
}

TEST_CASE("SampleIterator identifies a PERF_RECORD_SAMPLE record", "[SampleIterator]")
{
  const auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, {});
  const auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  REQUIRE(iterator.is_sample_event());
  REQUIRE_FALSE(iterator.is_lost_event());
  REQUIRE_FALSE(iterator.is_throttle_event());
}

TEST_CASE("SampleIterator identifies a PERF_RECORD_LOST record", "[SampleIterator]")
{
  const auto buffer = make_record(PERF_RECORD_LOST, 0U, {});
  const auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  REQUIRE(iterator.is_lost_event());
  REQUIRE_FALSE(iterator.is_sample_event());
}

TEST_CASE("SampleIterator distinguishes throttle from unthrottle records", "[SampleIterator]")
{
  const auto throttle_buffer = make_record(PERF_RECORD_THROTTLE, 0U, {});
  const auto throttle_iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(throttle_buffer.data()) };
  REQUIRE(throttle_iterator.is_throttle_event());
  REQUIRE(throttle_iterator.is_throttle());

  const auto unthrottle_buffer = make_record(PERF_RECORD_UNTHROTTLE, 0U, {});
  const auto unthrottle_iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(unthrottle_buffer.data()) };
  REQUIRE(unthrottle_iterator.is_throttle_event());
  REQUIRE_FALSE(unthrottle_iterator.is_throttle());
}

TEST_CASE("SampleIterator is_instruction_pointer_exact reflects the PERF_RECORD_MISC_EXACT_IP flag", "[SampleIterator]")
{
  const auto exact_buffer = make_record(PERF_RECORD_SAMPLE, PERF_RECORD_MISC_EXACT_IP, {});
  const auto exact_iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(exact_buffer.data()) };
  REQUIRE(exact_iterator.is_instruction_pointer_exact());

  const auto inexact_buffer = make_record(PERF_RECORD_SAMPLE, 0U, {});
  const auto inexact_iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(inexact_buffer.data()) };
  REQUIRE_FALSE(inexact_iterator.is_instruction_pointer_exact());
}

TEST_CASE("SampleIterator size returns the record's declared total size", "[SampleIterator]")
{
  const auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, u64_payload(1U));
  const auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  REQUIRE(iterator.size() == sizeof(perf_event_header) + sizeof(std::uint64_t));
}

TEST_CASE("SampleIterator remaining reflects unread payload bytes", "[SampleIterator]")
{
  const auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, u64_payload(1U));
  const auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  REQUIRE(iterator.remaining() == sizeof(std::uint64_t));
}

TEST_CASE("SampleIterator remaining underflows to a very large value when the declared size is smaller than "
          "the header itself",
          "[SampleIterator]")
{
  /// A declared record size smaller than sizeof(perf_event_header) can never occur on the wire — the
  /// header is always fully present. remaining() computes (header_address + declared_size) - data_address,
  /// where data_address is unconditionally header_address + sizeof(perf_event_header); this formula does
  /// not guard against an undersized declaration, so the subtraction underflows instead of going negative.
  auto header = perf_event_header{};
  header.type = PERF_RECORD_SAMPLE;
  header.misc = 0U;
  header.size = 4U;

  auto buffer = std::vector<std::byte>(sizeof(header), std::byte{ 0 });
  std::memcpy(buffer.data(), &header, sizeof(header));
  const auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  REQUIRE(iterator.remaining() > (std::numeric_limits<std::size_t>::max() / 2U));
}

TEST_CASE("SampleIterator read<T> returns the value and advances past it", "[SampleIterator]")
{
  constexpr auto expected = std::uint64_t{ 0xDEADBEEFCAFEBABEULL };
  auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, u64_payload(expected));
  auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  REQUIRE(iterator.remaining() == sizeof(std::uint64_t));
  const auto value = iterator.read<std::uint64_t>();
  REQUIRE(value == expected);
  REQUIRE(iterator.remaining() == 0U);
}

TEST_CASE("SampleIterator skip<T> advances without returning a value", "[SampleIterator]")
{
  auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, u64_payload(123U));
  auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  REQUIRE(iterator.remaining() == sizeof(std::uint64_t));
  iterator.skip<std::uint64_t>();
  REQUIRE(iterator.remaining() == 0U);
}

TEST_CASE("SampleIterator read_array<T> with size zero returns nullptr and does not advance", "[SampleIterator]")
{
  auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, u64_payload(0U));
  auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  const auto* array_ptr = iterator.read_array<std::uint32_t>(0U);
  REQUIRE(array_ptr == nullptr);
  REQUIRE(iterator.remaining() == sizeof(std::uint64_t));
}

TEST_CASE("SampleIterator read_array<T> returns the elements and advances by size * sizeof(T)", "[SampleIterator]")
{
  const auto values = std::array<std::uint32_t, 3U>{ 10U, 20U, 30U };
  auto payload = std::vector<std::byte>(sizeof(values));
  std::memcpy(payload.data(), values.data(), sizeof(values));

  auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, payload);
  auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  const auto* array_ptr = iterator.read_array<std::uint32_t>(3U);
  REQUIRE(array_ptr != nullptr);
  REQUIRE(array_ptr[0] == 10U);
  REQUIRE(array_ptr[1] == 20U);
  REQUIRE(array_ptr[2] == 30U);
  REQUIRE(iterator.remaining() == 0U);
}

TEST_CASE("SampleIterator as<T> returns the current data pointer without advancing", "[SampleIterator]")
{
  auto buffer = make_record(PERF_RECORD_SAMPLE, 0U, u64_payload(42U));
  auto iterator = perf::SampleIterator{ reinterpret_cast<std::uintptr_t>(buffer.data()) };

  const auto* data_ptr = iterator.as<const std::byte*>();
  REQUIRE(reinterpret_cast<const void*>(data_ptr) ==
          reinterpret_cast<const void*>(buffer.data() + sizeof(perf_event_header)));
  REQUIRE(iterator.remaining() == sizeof(std::uint64_t));
}

TEST_CASE("SampleDecoder decode on an empty buffer list returns no samples", "[SampleDecoder]")
{
  auto counter_definition = perf::CounterDefinition{};
  auto recording_values = perf::SampleRecordingValues{};
  const auto decoder = perf::SampleDecoder{ counter_definition, recording_values };

  auto requested_event_set = perf::RequestedEventSet{};
  auto group = perf::Group{};

  const auto samples = decoder.decode({}, false, false, requested_event_set, group);
  REQUIRE(samples.empty());
}

TEST_CASE("SampleDecoder skips a record of an unrecognized type", "[SampleDecoder]")
{
  auto counter_definition = perf::CounterDefinition{};
  auto recording_values = perf::SampleRecordingValues{};
  const auto decoder = perf::SampleDecoder{ counter_definition, recording_values };

  auto requested_event_set = perf::RequestedEventSet{};
  auto group = perf::Group{};

  /// 255 does not correspond to any PERF_RECORD_* constant defined by the kernel ABI.
  const auto buffer = make_record(255U, 0U, {});

  const auto samples = decoder.decode({ buffer }, false, false, requested_event_set, group);
  REQUIRE(samples.empty());
}

TEST_CASE("SampleDecoder decodes a throttle record", "[SampleDecoder]")
{
  auto counter_definition = perf::CounterDefinition{};
  auto recording_values = perf::SampleRecordingValues{}.throttle(true);
  const auto decoder = perf::SampleDecoder{ counter_definition, recording_values };

  auto requested_event_set = perf::RequestedEventSet{};
  auto group = perf::Group{};

  /// The documented PERF_RECORD_THROTTLE payload: time, id, stream_id (each u64).
  const auto payload = concat({ u64_payload(1000U), u64_payload(2000U), u64_payload(3000U) });
  const auto buffer = make_record(PERF_RECORD_THROTTLE, 0U, payload);

  const auto samples = decoder.decode({ buffer }, false, false, requested_event_set, group);

  REQUIRE(samples.size() == 1U);
  REQUIRE(samples.front().throttle().has_value());
  REQUIRE(samples.front().throttle()->is_throttle());
}

TEST_CASE("SampleDecoder decodes an unthrottle record", "[SampleDecoder]")
{
  auto counter_definition = perf::CounterDefinition{};
  auto recording_values = perf::SampleRecordingValues{}.throttle(true);
  const auto decoder = perf::SampleDecoder{ counter_definition, recording_values };

  auto requested_event_set = perf::RequestedEventSet{};
  auto group = perf::Group{};

  const auto payload = concat({ u64_payload(1000U), u64_payload(2000U), u64_payload(3000U) });
  const auto buffer = make_record(PERF_RECORD_UNTHROTTLE, 0U, payload);

  const auto samples = decoder.decode({ buffer }, false, false, requested_event_set, group);

  REQUIRE(samples.size() == 1U);
  REQUIRE(samples.front().throttle().has_value());
  REQUIRE(samples.front().throttle()->is_unthrottle());
}

TEST_CASE("SampleDecoder ignores a throttle record when throttle sampling was not requested", "[SampleDecoder]")
{
  auto counter_definition = perf::CounterDefinition{};
  /// Field::Throttle is not activated here, unlike the two tests above.
  auto recording_values = perf::SampleRecordingValues{};
  const auto decoder = perf::SampleDecoder{ counter_definition, recording_values };

  auto requested_event_set = perf::RequestedEventSet{};
  auto group = perf::Group{};

  const auto payload = concat({ u64_payload(1000U), u64_payload(2000U), u64_payload(3000U) });
  const auto buffer = make_record(PERF_RECORD_THROTTLE, 0U, payload);

  const auto samples = decoder.decode({ buffer }, false, false, requested_event_set, group);
  REQUIRE(samples.empty());
}
