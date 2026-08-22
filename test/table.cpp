#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <perfcpp/util/table.hpp>
#include <string>
#include <vector>

namespace {
/// Splits text into non-empty lines, tolerating an optional trailing newline.
std::vector<std::string>
split_lines(const std::string& text)
{
  auto lines = std::vector<std::string>{};

  auto start = std::size_t{ 0U };
  while (start < text.size()) {
    const auto end = text.find('\n', start);
    if (end == std::string::npos) {
      lines.push_back(text.substr(start));
      break;
    }

    lines.push_back(text.substr(start, end - start));
    start = end + 1U;
  }

  return lines;
}

/// Strips leading and trailing spaces.
std::string
trim(const std::string& text)
{
  const auto begin = text.find_first_not_of(' ');
  if (begin == std::string::npos) {
    return {};
  }

  const auto end = text.find_last_not_of(' ');
  return text.substr(begin, end - begin + 1U);
}
}

TEST_CASE("empty table produces empty output", "[Table]")
{
  const auto table = perf::util::Table{};
  REQUIRE(table.to_string().empty());
}

TEST_CASE("table with only a header row is not empty", "[Table]")
{
  auto table = perf::util::Table{};
  auto header_row = std::vector<perf::util::Table::Header>{};
  header_row.emplace_back(std::string{ "Name" });
  table.add(std::move(header_row));

  REQUIRE_FALSE(table.to_string().empty());
}

TEST_CASE("single row single column output contains its text", "[Table]")
{
  auto table = perf::util::Table{};
  auto row = perf::util::Table::Row{};
  row << std::string{ "hello" };
  table.add(std::move(row));

  REQUIRE(table.to_string().find("hello") != std::string::npos);
}

TEST_CASE("row count matches number of output lines", "[Table]")
{
  auto table = perf::util::Table{};
  for (auto index = 0U; index < 3U; ++index) {
    auto row = perf::util::Table::Row{};
    row << std::string{ "row" } << index;
    table.add(std::move(row));
  }

  const auto lines = split_lines(table.to_string());
  REQUIRE(lines.size() == 3U);
}

TEST_CASE("columns within a row are printed left-to-right in insertion order", "[Table]")
{
  auto table = perf::util::Table{};
  auto row = perf::util::Table::Row{};
  row << std::string{ "first" } << std::string{ "second" } << std::string{ "third" };
  table.add(std::move(row));

  const auto output = table.to_string();
  const auto pos_first = output.find("first");
  const auto pos_second = output.find("second");
  const auto pos_third = output.find("third");

  REQUIRE(pos_first != std::string::npos);
  REQUIRE(pos_second != std::string::npos);
  REQUIRE(pos_third != std::string::npos);
  REQUIRE(pos_first < pos_second);
  REQUIRE(pos_second < pos_third);
}

TEST_CASE("rows are printed in insertion order", "[Table]")
{
  auto table = perf::util::Table{};

  auto first_row = perf::util::Table::Row{};
  first_row << std::string{ "alpha" };
  table.add(std::move(first_row));

  auto second_row = perf::util::Table::Row{};
  second_row << std::string{ "beta" };
  table.add(std::move(second_row));

  const auto output = table.to_string();
  const auto pos_alpha = output.find("alpha");
  const auto pos_beta = output.find("beta");

  REQUIRE(pos_alpha != std::string::npos);
  REQUIRE(pos_beta != std::string::npos);
  REQUIRE(pos_alpha < pos_beta);
}

TEST_CASE("Row operator<< accepts numeric types and stringifies them", "[Table]")
{
  auto table = perf::util::Table{};
  auto row = perf::util::Table::Row{};
  row << std::size_t{ 42U } << std::uint32_t{ 7U } << 3.5F;
  table.add(std::move(row));

  const auto output = table.to_string();
  REQUIRE(output.find("42") != std::string::npos);
  REQUIRE(output.find('7') != std::string::npos);
}

TEST_CASE("row offset adds a fixed number of leading characters to every line", "[Table]")
{
  const auto build_single_row_output = [](const std::uint64_t offset) {
    auto table = perf::util::Table{ offset };
    auto row = perf::util::Table::Row{};
    row << std::string{ "x" };
    table.add(std::move(row));
    return table.to_string();
  };

  const auto without_offset = split_lines(build_single_row_output(0U));
  const auto with_offset = split_lines(build_single_row_output(5U));

  REQUIRE(without_offset.size() == 1U);
  REQUIRE(with_offset.size() == 1U);

  /// The offset must only add leading characters; the visible (trimmed) content is unchanged.
  REQUIRE(with_offset.front().size() == without_offset.front().size() + 5U);
  REQUIRE(trim(with_offset.front()) == trim(without_offset.front()));
}

TEST_CASE("columns are padded to the width of their widest entry", "[Table]")
{
  auto table = perf::util::Table{};

  auto short_row = perf::util::Table::Row{};
  short_row << std::string{ "A" } << std::string{ "short" } << std::string{ "END" };
  table.add(std::move(short_row));

  auto long_row = perf::util::Table::Row{};
  long_row << std::string{ "A" } << std::string{ "a-much-longer-value" } << std::string{ "END" };
  table.add(std::move(long_row));

  const auto lines = split_lines(table.to_string());
  REQUIRE(lines.size() == 2U);

  /// Regardless of alignment, the third column ("END") must start at the same offset in both
  /// lines since the middle column is padded to the width of its widest entry.
  const auto end_marker_position_short = lines[0].find("END");
  const auto end_marker_position_long = lines[1].find("END");

  REQUIRE(end_marker_position_short != std::string::npos);
  REQUIRE(end_marker_position_long != std::string::npos);
  REQUIRE(end_marker_position_short == end_marker_position_long);
}

TEST_CASE("right-aligned header right-justifies shorter column values", "[Table]")
{
  auto table = perf::util::Table{};
  auto header_row = std::vector<perf::util::Table::Header>{};
  header_row.emplace_back(std::string{ "NUM" }, perf::util::Table::Alignment::Right);
  table.add(std::move(header_row));

  auto short_row = perf::util::Table::Row{};
  short_row << std::string{ "5" };
  table.add(std::move(short_row));

  auto long_row = perf::util::Table::Row{};
  long_row << std::string{ "500" };
  table.add(std::move(long_row));

  const auto lines = split_lines(table.to_string());

  std::string line_with_5;
  std::string line_with_500;
  for (const auto& line : lines) {
    if (trim(line) == "5") {
      line_with_5 = line;
    } else if (trim(line) == "500") {
      line_with_500 = line;
    }
  }

  REQUIRE_FALSE(line_with_5.empty());
  REQUIRE_FALSE(line_with_500.empty());

  /// Right-aligned values end at the same column position, independent of their own width.
  const auto end_of_5 = line_with_5.find('5') + 1U;
  const auto end_of_500 = line_with_500.find("500") + 3U;
  REQUIRE(end_of_5 == end_of_500);
}

TEST_CASE("left-aligned header left-justifies shorter column values", "[Table]")
{
  auto table = perf::util::Table{};
  auto header_row = std::vector<perf::util::Table::Header>{};
  header_row.emplace_back(std::string{ "NUM" }, perf::util::Table::Alignment::Left);
  table.add(std::move(header_row));

  auto short_row = perf::util::Table::Row{};
  short_row << std::string{ "5" };
  table.add(std::move(short_row));

  auto long_row = perf::util::Table::Row{};
  long_row << std::string{ "500" };
  table.add(std::move(long_row));

  const auto lines = split_lines(table.to_string());

  std::string line_with_5;
  std::string line_with_500;
  for (const auto& line : lines) {
    if (trim(line) == "5") {
      line_with_5 = line;
    } else if (trim(line) == "500") {
      line_with_500 = line;
    }
  }

  REQUIRE_FALSE(line_with_5.empty());
  REQUIRE_FALSE(line_with_500.empty());

  /// Left-aligned values start at the same column position, independent of their own width.
  const auto start_of_5 = line_with_5.find('5');
  const auto start_of_500 = line_with_500.find("500");
  REQUIRE(start_of_5 == start_of_500);
}

TEST_CASE("reserve does not change observable output", "[Table]")
{
  auto table = perf::util::Table{};
  table.reserve(100U);

  auto row = perf::util::Table::Row{};
  row << std::string{ "hello" };
  table.add(std::move(row));

  REQUIRE(table.to_string().find("hello") != std::string::npos);
}
