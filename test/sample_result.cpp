#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <perfcpp/sample/result.hpp>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string>
split_line(const std::string& line, const char delimiter)
{
  auto tokens = std::vector<std::string>{};
  auto stream = std::istringstream{ line };
  auto token = std::string{};
  while (std::getline(stream, token, delimiter)) {
    tokens.push_back(token);
  }
  /// std::getline does not produce a trailing empty token when the line ends
  /// with the delimiter — add it explicitly so callers see an empty cell.
  if (!line.empty() && line.back() == delimiter) {
    tokens.push_back("");
  }
  return tokens;
}

std::vector<std::vector<std::string>>
parse_csv(const std::string& csv, const char delimiter = ',')
{
  auto rows = std::vector<std::vector<std::string>>{};
  auto stream = std::istringstream{ csv };
  auto line = std::string{};
  while (std::getline(stream, line)) {
    if (!line.empty()) {
      rows.push_back(split_line(line, delimiter));
    }
  }
  return rows;
}
}

TEST_CASE("to_csv", "[SampleResult]")
{
  SECTION("no fields, no samples: only mode column")
  {
    const auto result = perf::SampleResult{ perf::SampleRecordingValues{}, {} };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 1U);
    REQUIRE(rows[0].size() == 1U);
    REQUIRE(rows[0][0] == "mode");
  }

  SECTION("id field")
  {
    auto values = perf::SampleRecordingValues{}.id(true);

    auto sample = perf::Sample{};
    sample.metadata().sample_id(42ULL);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "id");
    REQUIRE(rows[1][1] == "42");
  }

  SECTION("stream_id field")
  {
    auto values = perf::SampleRecordingValues{}.stream_id(true);

    auto sample = perf::Sample{};
    sample.metadata().stream_id(99ULL);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "stream_id");
    REQUIRE(rows[1][1] == "99");
  }

  SECTION("timestamp field")
  {
    auto values = perf::SampleRecordingValues{}.timestamp(true);

    auto sample = perf::Sample{};
    sample.metadata().mode(perf::Metadata::Mode::User);
    sample.metadata().timestamp(1234567890ULL);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "timestamp");
    REQUIRE(rows[1][0] == "user");
    REQUIRE(rows[1][1] == "1234567890");
  }

  SECTION("period field")
  {
    auto values = perf::SampleRecordingValues{}.period(true);

    auto sample = perf::Sample{};
    sample.metadata().period(4000ULL);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "period");
    REQUIRE(rows[1][1] == "4000");
  }

  SECTION("cpu_id field")
  {
    auto values = perf::SampleRecordingValues{}.cpu_id(true);

    auto sample = perf::Sample{};
    sample.metadata().cpu_id(3U);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "cpu_id");
    REQUIRE(rows[1][1] == "3");
  }

  SECTION("thread_id field produces process_id and thread_id columns")
  {
    auto values = perf::SampleRecordingValues{}.thread_id(true);

    auto sample = perf::Sample{};
    sample.metadata().mode(perf::Metadata::Mode::Kernel);
    sample.metadata().process_id(100U);
    sample.metadata().thread_id(200U);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "process_id");
    REQUIRE(rows[0][2] == "thread_id");
    REQUIRE(rows[1][0] == "kernel");
    REQUIRE(rows[1][1] == "100");
    REQUIRE(rows[1][2] == "200");
  }

  SECTION("logical_instruction_pointer field produces address and exactness columns")
  {
    auto values = perf::SampleRecordingValues{}.logical_instruction_pointer(true);

    auto sample = perf::Sample{};
    sample.instruction_execution().logical_instruction_pointer(0xdeadbeefULL);
    sample.instruction_execution().instruction_pointer_exact(true);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "logical_instruction_pointer");
    REQUIRE(rows[0][2] == "is_instruction_pointer_exact");

    /// Address is written in hex; bool is written as "true" / "false".
    REQUIRE(rows[1][1] == "0xdeadbeef");
    REQUIRE(rows[1][2] == "true");
  }

  SECTION("logical_memory_address field")
  {
    auto values = perf::SampleRecordingValues{}.logical_memory_address(true);

    auto sample = perf::Sample{};
    sample.data_access().logical_memory_address(0xcafebabeULL);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "logical_memory_address");
    REQUIRE(rows[1][1] == "0xcafebabe");
  }

  SECTION("user registers produce one column per register")
  {
    auto values = perf::SampleRecordingValues{}.user_registers(
      std::vector<perf::Registers::x86>{ perf::Registers::x86::AX, perf::Registers::x86::IP });

    auto sample = perf::Sample{};
    sample.user_registers(perf::RegisterValues{ perf::ABI::Regs64,
                                                { { static_cast<std::uint8_t>(perf::Registers::x86::AX), 100LL },
                                                  { static_cast<std::uint8_t>(perf::Registers::x86::IP), 200LL } } });

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    /// AX (enum value 0) sorts before IP (enum value 8).
    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "user_register_ax");
    REQUIRE(rows[0][2] == "user_register_ip");
    REQUIRE(rows[1][1] == "100");
    REQUIRE(rows[1][2] == "200");
  }

  SECTION("kernel registers produce one column per register")
  {
    auto values = perf::SampleRecordingValues{}.kernel_registers(
      std::vector<perf::Registers::x86>{ perf::Registers::x86::SP, perf::Registers::x86::BP });

    auto sample = perf::Sample{};
    sample.kernel_registers(
      perf::RegisterValues{ perf::ABI::Regs64,
                            { { static_cast<std::uint8_t>(perf::Registers::x86::SP), 0xffffULL },
                              { static_cast<std::uint8_t>(perf::Registers::x86::BP), 0xeeeeULL } } });

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    /// BP (enum value 6) sorts before SP (enum value 7).
    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "kernel_register_bp");
    REQUIRE(rows[0][2] == "kernel_register_sp");
    REQUIRE(rows[1][1] == std::to_string(0xeeeeLL));
    REQUIRE(rows[1][2] == std::to_string(0xffffLL));
  }

  SECTION("counter values produce one column per counter")
  {
    auto values = perf::SampleRecordingValues{}.counter({ "instructions", "cycles" });

    auto counter_result = perf::CounterResult{};
    counter_result.emplace_back("instructions", 1000.0);
    counter_result.emplace_back("cycles", 2000.0);

    auto sample = perf::Sample{};
    sample.counter(std::move(counter_result));

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][1] == "counter_instructions");
    REQUIRE(rows[0][2] == "counter_cycles");
    REQUIRE(rows[1][1] == "1000");
    REQUIRE(rows[1][2] == "2000");
  }

  SECTION("missing counter result writes empty cells")
  {
    auto values = perf::SampleRecordingValues{}.counter({ "instructions", "cycles" });

    /// Sample has no CounterResult set at all.
    auto samples = std::vector<perf::Sample>{ perf::Sample{} };

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[1].size() == 3U);
    REQUIRE(rows[1][1].empty());
    REQUIRE(rows[1][2].empty());
  }

  SECTION("missing register value writes empty cell")
  {
    auto values = perf::SampleRecordingValues{}.user_registers(
      std::vector<perf::Registers::x86>{ perf::Registers::x86::AX, perf::Registers::x86::IP });

    /// Only AX is populated; IP is absent from the map.
    auto sample = perf::Sample{};
    sample.user_registers(
      perf::RegisterValues{ perf::ABI::Regs64, { { static_cast<std::uint8_t>(perf::Registers::x86::AX), 42LL } } });

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[1].size() == 3U);
    REQUIRE(rows[1][1] == "42");
    REQUIRE(rows[1][2].empty());
  }

  SECTION("multiple samples produce one row per sample")
  {
    auto values = perf::SampleRecordingValues{}.timestamp(true);

    auto samples = std::vector<perf::Sample>{};
    for (auto i = 0U; i < 3U; ++i) {
      auto sample = perf::Sample{};
      sample.metadata().timestamp(static_cast<std::uint64_t>(i) * 1000U);
      samples.push_back(std::move(sample));
    }

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 4U);
    REQUIRE(rows[1][1] == "0");
    REQUIRE(rows[2][1] == "1000");
    REQUIRE(rows[3][1] == "2000");
  }
}

TEST_CASE("to_csv file", "[SampleResult]")
{
  constexpr auto file_path = "/tmp/perfcpp_sample_result_test.csv";

  SECTION("file content matches string overload")
  {
    auto values = perf::SampleRecordingValues{}.timestamp(true);

    auto sample = perf::Sample{};
    sample.metadata().mode(perf::Metadata::Mode::User);
    sample.metadata().timestamp(9876543210ULL);

    auto samples = std::vector<perf::Sample>{};
    samples.push_back(std::move(sample));

    const auto result = perf::SampleResult{ std::move(values), std::move(samples) };
    result.to_csv(file_path);

    auto file = std::ifstream{ file_path };
    REQUIRE(file.is_open());
    const auto file_content = std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

    REQUIRE(file_content == result.to_csv());

    std::remove(file_path);
  }
}
