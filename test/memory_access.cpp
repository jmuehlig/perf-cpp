#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <numeric>
#include <perfcpp/analyzer/memory_access.hpp>
#include <perfcpp/exception.hpp>

namespace {

/// Builds a minimal sample with the given address and access type.
/// A DataAccess::Source must be present for the sample to be counted in statistics;
/// we always set one so tests are not sensitive to that implementation detail.
perf::Sample
make_sample(const std::uintptr_t address, const perf::DataAccess::AccessType type = perf::DataAccess::AccessType::Load)
{
  auto sample = perf::Sample{};
  sample.data_access().logical_memory_address(address);
  sample.data_access().type(type);
  sample.data_access().source(perf::DataAccess::Source{});
  return sample;
}
}

TEST_CASE("MemoryAccess add", "[MemoryAccess]")
{
  SECTION("registering the same type name twice throws DataTypeAlreadyRegisteredError")
  {
    auto analyzer = perf::analyzer::MemoryAccess{};

    auto data_type = perf::analyzer::DataType{ "T", 8U };
    data_type.add("field", 0U, 8U);
    analyzer.add(std::move(data_type));

    auto duplicate = perf::analyzer::DataType{ "T", 8U };
    REQUIRE_THROWS_AS(analyzer.add(std::move(duplicate)), perf::DataTypeAlreadyRegisteredError);
  }
}

TEST_CASE("MemoryAccess annotate", "[MemoryAccess]")
{
  SECTION("annotating an unregistered type throws DataTypeNotRegisteredError")
  {
    auto analyzer = perf::analyzer::MemoryAccess{};
    alignas(8) std::array<char, 8U> buffer{};
    REQUIRE_THROWS_AS(analyzer.annotate("Unregistered", buffer.data()), perf::DataTypeNotRegisteredError);
  }
}

TEST_CASE("MemoryAccess map routes samples to members by address", "[MemoryAccess]")
{
  /// 16-byte type with two adjacent 8-byte members.
  alignas(8) std::array<char, 16U> buffer{};
  const auto base = std::uintptr_t(buffer.data());

  auto data_type = perf::analyzer::DataType{ "T", 16U };
  data_type.add("first", 0U, 8U);
  data_type.add("second", 8U, 8U);

  auto analyzer = perf::analyzer::MemoryAccess{};
  analyzer.add(std::move(data_type));
  analyzer.annotate("T", buffer.data());

  SECTION("sample inside first member is attributed to first member only")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base + 4U) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();

    const auto first = std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "first"; });
    REQUIRE(first != members.end());
    REQUIRE(first->samples().size() == 1U);

    const auto second =
      std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "second"; });
    REQUIRE(second != members.end());
    REQUIRE(second->samples().empty());
  }

  SECTION("sample inside second member is attributed to second member only")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base + 12U) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();

    const auto second =
      std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "second"; });
    REQUIRE(second != members.end());
    REQUIRE(second->samples().size() == 1U);

    const auto first = std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "first"; });
    REQUIRE(first != members.end());
    REQUIRE(first->samples().empty());
  }

  SECTION("sample at exact start of instance maps to first member")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();

    const auto first = std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "first"; });
    REQUIRE(first != members.end());
    REQUIRE(first->samples().size() == 1U);
  }

  SECTION("sample at last byte of first member maps to first member")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base + 7U) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();

    const auto first = std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "first"; });
    REQUIRE(first != members.end());
    REQUIRE(first->samples().size() == 1U);
  }

  SECTION("sample at first byte of second member maps to second member")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base + 8U) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();

    const auto second =
      std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "second"; });
    REQUIRE(second != members.end());
    REQUIRE(second->samples().size() == 1U);
  }

  SECTION("sample one byte past end of instance is not attributed to any member")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base + 16U) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();
    const auto total =
      std::accumulate(members.begin(), members.end(), std::size_t{ 0U }, [](const auto sum, const auto& m) {
        return sum + m.samples().size();
      });
    REQUIRE(total == 0U);
  }

  SECTION("sample before start of instance is not attributed to any member")
  {
    REQUIRE(base > 0U);
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base - 1U) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();
    const auto total =
      std::accumulate(members.begin(), members.end(), std::size_t{ 0U }, [](const auto sum, const auto& m) {
        return sum + m.samples().size();
      });
    REQUIRE(total == 0U);
  }
}

TEST_CASE("MemoryAccess map fills structural gaps with unknown padding members", "[MemoryAccess]")
{
  /// 16-byte type: 4-byte member, intentional 4-byte gap, then 8-byte member.
  alignas(8) std::array<char, 16U> buffer{};
  const auto base = std::uintptr_t(buffer.data());

  auto data_type = perf::analyzer::DataType{ "Gapped", 16U };
  data_type.add("first", 0U, 4U);
  data_type.add("second", 8U, 8U);

  auto analyzer = perf::analyzer::MemoryAccess{};
  analyzer.add(std::move(data_type));
  analyzer.annotate("Gapped", buffer.data());

  SECTION("sample in the gap is attributed to an unknown padding member")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(base + 5U) });

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();

    const auto unknown =
      std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "/* unknown */"; });
    REQUIRE(unknown != members.end());
    REQUIRE(unknown->samples().size() == 1U);
  }

  SECTION("gap filling does not affect samples on declared members")
  {
    auto samples = std::vector<perf::Sample>{};
    samples.push_back(make_sample(base + 2U));  /// inside "first"
    samples.push_back(make_sample(base + 5U));  /// inside the gap
    samples.push_back(make_sample(base + 12U)); /// inside "second"
    const auto result = analyzer.map(samples);

    REQUIRE_FALSE(result.data_types().empty());
    const auto& members = result.data_types().front().members();

    const auto first = std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "first"; });
    REQUIRE(first != members.end());
    REQUIRE(first->samples().size() == 1U);

    const auto second =
      std::find_if(members.begin(), members.end(), [](const auto& m) { return m.name() == "second"; });
    REQUIRE(second != members.end());
    REQUIRE(second->samples().size() == 1U);
  }
}

TEST_CASE("MemoryAccess map separates instances by tag", "[MemoryAccess]")
{
  alignas(8) std::array<char, 8U> buf_a{};
  alignas(8) std::array<char, 8U> buf_b{};

  auto data_type = perf::analyzer::DataType{ "T", 8U };
  data_type.add("field", 0U, 8U);

  auto analyzer = perf::analyzer::MemoryAccess{};
  analyzer.add(std::move(data_type));
  analyzer.annotate("T", buf_a.data(), std::string{ "a" });
  analyzer.annotate("T", buf_b.data(), std::string{ "b" });

  SECTION("sample for tag 'a' is not attributed to tag 'b'")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(std::uintptr_t(buf_a.data()) + 4U) });

    const auto& data_types = result.data_types();
    const auto tag_b =
      std::find_if(data_types.begin(), data_types.end(), [](const auto& dt) { return dt.name() == "T::b"; });
    REQUIRE(tag_b != data_types.end());

    const auto total = std::accumulate(tag_b->members().begin(),
                                       tag_b->members().end(),
                                       std::size_t{ 0U },
                                       [](const auto sum, const auto& m) { return sum + m.samples().size(); });
    REQUIRE(total == 0U);
  }

  SECTION("sample for tag 'b' is not attributed to tag 'a'")
  {
    const auto result = analyzer.map(std::vector<perf::Sample>{ make_sample(std::uintptr_t(buf_b.data()) + 4U) });

    const auto& data_types = result.data_types();
    const auto tag_a =
      std::find_if(data_types.begin(), data_types.end(), [](const auto& dt) { return dt.name() == "T::a"; });
    REQUIRE(tag_a != data_types.end());

    const auto total = std::accumulate(tag_a->members().begin(),
                                       tag_a->members().end(),
                                       std::size_t{ 0U },
                                       [](const auto sum, const auto& m) { return sum + m.samples().size(); });
    REQUIRE(total == 0U);
  }

  SECTION("each tag receives only its own sample when both are fired")
  {
    auto samples = std::vector<perf::Sample>{};
    samples.push_back(make_sample(std::uintptr_t(buf_a.data())));
    samples.push_back(make_sample(std::uintptr_t(buf_b.data())));
    const auto result = analyzer.map(samples);

    const auto& data_types = result.data_types();
    const auto tag_a =
      std::find_if(data_types.begin(), data_types.end(), [](const auto& dt) { return dt.name() == "T::a"; });
    const auto tag_b =
      std::find_if(data_types.begin(), data_types.end(), [](const auto& dt) { return dt.name() == "T::b"; });

    REQUIRE(tag_a != data_types.end());
    REQUIRE(tag_b != data_types.end());

    const auto total_a = std::accumulate(tag_a->members().begin(),
                                         tag_a->members().end(),
                                         std::size_t{ 0U },
                                         [](const auto sum, const auto& m) { return sum + m.samples().size(); });
    const auto total_b = std::accumulate(tag_b->members().begin(),
                                         tag_b->members().end(),
                                         std::size_t{ 0U },
                                         [](const auto sum, const auto& m) { return sum + m.samples().size(); });

    REQUIRE(total_a == 1U);
    REQUIRE(total_b == 1U);
  }
}
