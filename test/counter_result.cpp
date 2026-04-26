#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <perfcpp/counter/result.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
/// Splits a single line on the given delimiter and returns the tokens.
std::vector<std::string>
split_line(const std::string& line, const char delimiter)
{
  auto tokens = std::vector<std::string>{};
  auto stream = std::istringstream{ line };
  auto token = std::string{};
  while (std::getline(stream, token, delimiter)) {
    tokens.push_back(token);
  }
  return tokens;
}

/// Parses a CSV string into rows of tokens, skipping empty lines.
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

/// Finds the value for a given key in a flat JSON object string and returns it as a double.
/// Returns nullopt if the key is absent or the value cannot be parsed.
std::optional<double>
find_json_value(const std::string& json, std::string_view key)
{
  const auto key_pattern = "\"" + std::string{ key } + "\"";
  const auto key_pos = json.find(key_pattern);
  if (key_pos == std::string::npos) {
    return std::nullopt;
  }

  const auto colon_pos = json.find(':', key_pos + key_pattern.size());
  if (colon_pos == std::string::npos) {
    return std::nullopt;
  }

  auto value_start = colon_pos + 1U;
  while (value_start < json.size() && json[value_start] == ' ') {
    ++value_start;
  }

  try {
    return std::stod(json.substr(value_start));
  } catch (const std::exception&) {
    return std::nullopt;
  }
}
}

TEST_CASE("access", "[CounterResult]")
{
  SECTION("empty result")
  {
    auto result = perf::CounterResult{};

    REQUIRE(result.empty());
    REQUIRE(result.size() == 0U);
    REQUIRE(result.begin() == result.end());
    REQUIRE_FALSE(result.get("instructions").has_value());
    REQUIRE_FALSE(result["instructions"].has_value());
  }

  SECTION("get by name")
  {
    auto result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "instructions", 100.0 },
                                                                                         { "cycles", 200.0 } } };

    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() == 100.0);
    REQUIRE(result.get("cycles").has_value());
    REQUIRE(result.get("cycles").value() == 200.0);
    REQUIRE_FALSE(result.get("nonexistent").has_value());
  }

  SECTION("get with package prefix")
  {
    auto result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "cycles", 42.0 } } };

    /// Single slash triggers fallback: "cpu/cycles" -> "cycles".
    REQUIRE(result.get("cpu/cycles").has_value());
    REQUIRE(result.get("cpu/cycles").value() == 42.0);

    /// Double slash does NOT trigger fallback.
    REQUIRE_FALSE(result.get("a/b/c").has_value());

    /// Exact match still works.
    REQUIRE(result.get("cycles").has_value());
  }

  SECTION("operator[]")
  {
    auto result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{ { "instructions", 100.0 },
                                                                                         { "cycles", 200.0 } } };

    REQUIRE(result["instructions"].has_value());
    REQUIRE(result["instructions"].value() == 100.0);
    REQUIRE_FALSE(result["missing"].has_value());
  }

  SECTION("iteration")
  {
    auto result = perf::CounterResult{ std::vector<std::pair<std::string_view, double>>{
      { "instructions", 100.0 }, { "cycles", 200.0 }, { "cache-misses", 50.0 } } };

    REQUIRE(result.size() == 3U);
    REQUIRE_FALSE(result.empty());

    /// Verify order is preserved.
    auto names = std::vector<std::string_view>{};
    auto values = std::vector<double>{};
    for (const auto& [name, value] : result) {
      names.push_back(name);
      values.push_back(value);
    }

    REQUIRE(names[0] == "instructions");
    REQUIRE(names[1] == "cycles");
    REQUIRE(names[2] == "cache-misses");
    REQUIRE(values[0] == 100.0);
    REQUIRE(values[1] == 200.0);
    REQUIRE(values[2] == 50.0);
  }

  SECTION("emplace_back")
  {
    auto result = perf::CounterResult{};
    REQUIRE(result.empty());

    result.emplace_back("instructions", 1.0);
    result.emplace_back("cycles", 2.0);

    REQUIRE(result.size() == 2U);
    REQUIRE(result.get("instructions").has_value());
    REQUIRE(result.get("instructions").value() == 1.0);
    REQUIRE(result.get("cycles").has_value());
    REQUIRE(result.get("cycles").value() == 2.0);
  }
}

TEST_CASE("to_json", "[CounterResult]")
{
  SECTION("empty result")
  {
    const auto json = perf::CounterResult{}.to_json();

    /// An empty result must still be a valid JSON object.
    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '}');
  }

  SECTION("single entry")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);

    const auto json = result.to_json();

    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '}');

    const auto value = find_json_value(json, "instructions");
    REQUIRE(value.has_value());
    REQUIRE(value.value() == 100.0);
  }

  SECTION("multiple entries")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);
    result.emplace_back("cycles", 200.0);
    result.emplace_back("cache-misses", 3.5);

    const auto json = result.to_json();

    REQUIRE(json.front() == '{');
    REQUIRE(json.back() == '}');

    const auto instructions = find_json_value(json, "instructions");
    REQUIRE(instructions.has_value());
    REQUIRE(instructions.value() == 100.0);

    const auto cycles = find_json_value(json, "cycles");
    REQUIRE(cycles.has_value());
    REQUIRE(cycles.value() == 200.0);

    const auto cache_misses = find_json_value(json, "cache-misses");
    REQUIRE(cache_misses.has_value());
    REQUIRE(cache_misses.value() == 3.5);
  }

  SECTION("absent key returns nullopt")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("cycles", 42.0);

    const auto json = result.to_json();

    REQUIRE_FALSE(find_json_value(json, "instructions").has_value());
  }
}

TEST_CASE("to_json file", "[CounterResult]")
{
  constexpr auto file_path = "/tmp/perfcpp_counter_result_test.json";

  SECTION("file content matches string overload")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);
    result.emplace_back("cycles", 200.0);

    result.to_json(file_path);

    auto file = std::ifstream{ file_path };
    REQUIRE(file.is_open());
    const auto file_content = std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

    REQUIRE(file_content == result.to_json());

    std::remove(file_path);
  }

  SECTION("empty result writes valid JSON object")
  {
    perf::CounterResult{}.to_json(file_path);

    auto file = std::ifstream{ file_path };
    REQUIRE(file.is_open());
    const auto file_content = std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

    REQUIRE(file_content.front() == '{');
    REQUIRE(file_content.back() == '}');

    std::remove(file_path);
  }
}

TEST_CASE("to_csv", "[CounterResult]")
{
  SECTION("empty result with header")
  {
    const auto rows = parse_csv(perf::CounterResult{}.to_csv());

    /// Only the header row, no data rows.
    REQUIRE(rows.size() == 1U);
    REQUIRE(rows[0][0] == "counter");
    REQUIRE(rows[0][1] == "value");
  }

  SECTION("empty result without header")
  {
    REQUIRE(perf::CounterResult{}.to_csv(',', false).empty());
  }

  SECTION("single entry with header")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);

    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][0] == "counter");
    REQUIRE(rows[0][1] == "value");
    REQUIRE(rows[1][0] == "instructions");
    REQUIRE(std::stod(rows[1][1]) == 100.0);
  }

  SECTION("multiple entries with header")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);
    result.emplace_back("cycles", 200.0);
    result.emplace_back("cache-misses", 3.5);

    const auto rows = parse_csv(result.to_csv());

    REQUIRE(rows.size() == 4U);
    REQUIRE(rows[0][0] == "counter");
    REQUIRE(rows[0][1] == "value");
    REQUIRE(rows[1][0] == "instructions");
    REQUIRE(std::stod(rows[1][1]) == 100.0);
    REQUIRE(rows[2][0] == "cycles");
    REQUIRE(std::stod(rows[2][1]) == 200.0);
    REQUIRE(rows[3][0] == "cache-misses");
    REQUIRE(std::stod(rows[3][1]) == 3.5);
  }

  SECTION("no header")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);
    result.emplace_back("cycles", 200.0);

    const auto rows = parse_csv(result.to_csv(',', false));

    /// First row is data, not a header.
    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][0] == "instructions");
    REQUIRE(std::stod(rows[0][1]) == 100.0);
    REQUIRE(rows[1][0] == "cycles");
    REQUIRE(std::stod(rows[1][1]) == 200.0);
  }

  SECTION("custom delimiter")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);
    result.emplace_back("cycles", 200.0);

    const auto rows = parse_csv(result.to_csv(';'), ';');

    REQUIRE(rows.size() == 3U);
    REQUIRE(rows[0][0] == "counter");
    REQUIRE(rows[0][1] == "value");
    REQUIRE(rows[1][0] == "instructions");
    REQUIRE(std::stod(rows[1][1]) == 100.0);
    REQUIRE(rows[2][0] == "cycles");
    REQUIRE(std::stod(rows[2][1]) == 200.0);
  }
}

TEST_CASE("to_csv file", "[CounterResult]")
{
  constexpr auto file_path = "/tmp/perfcpp_counter_result_test.csv";

  SECTION("file content matches string overload")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);
    result.emplace_back("cycles", 200.0);

    result.to_csv(file_path);

    auto file = std::ifstream{ file_path };
    REQUIRE(file.is_open());
    const auto file_content = std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

    REQUIRE(file_content == result.to_csv());

    std::remove(file_path);
  }

  SECTION("custom delimiter written to file")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);

    result.to_csv(file_path, ';');

    auto file = std::ifstream{ file_path };
    REQUIRE(file.is_open());
    const auto file_content = std::string{ std::istreambuf_iterator<char>{ file }, std::istreambuf_iterator<char>{} };

    const auto rows = parse_csv(file_content, ';');
    REQUIRE(rows.size() == 2U);
    REQUIRE(rows[0][0] == "counter");
    REQUIRE(rows[1][0] == "instructions");

    std::remove(file_path);
  }
}

TEST_CASE("to_string", "[CounterResult]")
{
  SECTION("empty result")
  {
    REQUIRE(perf::CounterResult{}.to_string() == "No results collected.");
  }

  SECTION("each name and value appears in output")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);
    result.emplace_back("cycles", 200.0);
    result.emplace_back("cache-misses", 3.5);

    const auto output = result.to_string();

    REQUIRE(output.find("instructions") != std::string::npos);
    REQUIRE(output.find(std::to_string(100.0)) != std::string::npos);
    REQUIRE(output.find("cycles") != std::string::npos);
    REQUIRE(output.find(std::to_string(200.0)) != std::string::npos);
    REQUIRE(output.find("cache-misses") != std::string::npos);
    REQUIRE(output.find(std::to_string(3.5)) != std::string::npos);
  }

  SECTION("name appears after value on each line")
  {
    auto result = perf::CounterResult{};
    result.emplace_back("instructions", 100.0);

    const auto output = result.to_string();
    const auto value_pos = output.find(std::to_string(100.0));
    const auto name_pos = output.find("instructions");

    REQUIRE(value_pos != std::string::npos);
    REQUIRE(name_pos != std::string::npos);
    REQUIRE(name_pos > value_pos);
  }
}
