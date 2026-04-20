#include <catch2/catch_test_macros.hpp>
#include <perfcpp/counter/counter.hpp>
#include <perfcpp/sample/period.hpp>
#include <perfcpp/sample/precision.hpp>

TEST_CASE("CounterConfig construction", "[CounterConfig]")
{
  SECTION("type and id are stored")
  {
    const auto config = perf::CounterConfig{ 4U, 42UL };

    REQUIRE(config.type() == 4U);
    REQUIRE(config.configs()[0U] == 42UL);
  }

  SECTION("id extensions default to zero")
  {
    const auto config = perf::CounterConfig{ 1U, 10UL };

    REQUIRE(config.configs()[1U] == 0UL);
    REQUIRE(config.configs()[2U] == 0UL);
  }

  SECTION("id extensions are stored when provided")
  {
    const auto config = perf::CounterConfig{ 1U, 10UL, 20UL, 30UL };

    REQUIRE(config.configs()[0U] == 10UL);
    REQUIRE(config.configs()[1U] == 20UL);
    REQUIRE(config.configs()[2U] == 30UL);
  }

  SECTION("is_fixed defaults to false")
  {
    const auto config = perf::CounterConfig{ 1U, 10UL };
    REQUIRE_FALSE(config.is_fixed());
  }

  SECTION("is_fixed set to true via constructor")
  {
    const auto config = perf::CounterConfig{ 1U, 10UL, 0UL, 0UL, true };
    REQUIRE(config.is_fixed());
  }
}

TEST_CASE("CounterConfig defaults", "[CounterConfig]")
{
  const auto config = perf::CounterConfig{ 1U, 10UL };

  SECTION("scale defaults to 1.0")
  {
    REQUIRE(config.scale() == 1.0);
  }

  SECTION("precision defaults to nullopt")
  {
    REQUIRE_FALSE(config.precision().has_value());
  }

  SECTION("period_or_frequency defaults to nullopt")
  {
    REQUIRE_FALSE(config.period_or_frequency().has_value());
  }
}

TEST_CASE("CounterConfig fixed", "[CounterConfig]")
{
  SECTION("fixed mutator sets to true")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.fixed(true);
    REQUIRE(config.is_fixed());
  }

  SECTION("fixed mutator sets to false")
  {
    auto config = perf::CounterConfig{ 1U, 10UL, 0UL, 0UL, true };
    config.fixed(false);
    REQUIRE_FALSE(config.is_fixed());
  }
}

TEST_CASE("CounterConfig scale", "[CounterConfig]")
{
  SECTION("scale mutator roundtrip")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.scale(2.5);
    REQUIRE(config.scale() == 2.5);
  }

  SECTION("scale can be less than one")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.scale(0.5);
    REQUIRE(config.scale() == 0.5);
  }

  SECTION("scale can be greater than one")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.scale(100.0);
    REQUIRE(config.scale() == 100.0);
  }
}

TEST_CASE("CounterConfig precision", "[CounterConfig]")
{
  SECTION("precision from uint8_t")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.precision(std::uint8_t{ 2U });

    REQUIRE(config.precision().has_value());
    REQUIRE(config.precision().value() == 2U);
  }

  SECTION("AllowArbitrarySkid maps to 0")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.precision(perf::Precision::AllowArbitrarySkid);
    REQUIRE(config.precision().has_value());
    REQUIRE(config.precision().value() == 0U);
  }

  SECTION("MustHaveConstantSkid maps to 1")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.precision(perf::Precision::MustHaveConstantSkid);
    REQUIRE(config.precision().has_value());
    REQUIRE(config.precision().value() == 1U);
  }

  SECTION("RequestZeroSkid maps to 2")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.precision(perf::Precision::RequestZeroSkid);
    REQUIRE(config.precision().has_value());
    REQUIRE(config.precision().value() == 2U);
  }

  SECTION("MustHaveZeroSkid maps to 3")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.precision(perf::Precision::MustHaveZeroSkid);
    REQUIRE(config.precision().has_value());
    REQUIRE(config.precision().value() == 3U);
  }
}

TEST_CASE("CounterConfig period_or_frequency", "[CounterConfig]")
{
  SECTION("set Period")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.period_or_frequency(perf::Period{ 50000UL });

    REQUIRE(config.period_or_frequency().has_value());
    REQUIRE(std::holds_alternative<perf::Period>(config.period_or_frequency().value()));
    REQUIRE(std::get<perf::Period>(config.period_or_frequency().value()).get() == 50000UL);
  }

  SECTION("set Frequency")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.period_or_frequency(perf::Frequency{ 1000UL });

    REQUIRE(config.period_or_frequency().has_value());
    REQUIRE(std::holds_alternative<perf::Frequency>(config.period_or_frequency().value()));
    REQUIRE(std::get<perf::Frequency>(config.period_or_frequency().value()).get() == 1000UL);
  }

  SECTION("overwriting Period with Frequency")
  {
    auto config = perf::CounterConfig{ 1U, 10UL };
    config.period_or_frequency(perf::Period{ 50000UL });
    config.period_or_frequency(perf::Frequency{ 1000UL });

    REQUIRE(config.period_or_frequency().has_value());
    REQUIRE(std::holds_alternative<perf::Frequency>(config.period_or_frequency().value()));
    REQUIRE(std::get<perf::Frequency>(config.period_or_frequency().value()).get() == 1000UL);
  }
}

TEST_CASE("CounterConfig equality", "[CounterConfig]")
{
  SECTION("equal when type and config[0] match")
  {
    const auto a = perf::CounterConfig{ 4U, 42UL };
    const auto b = perf::CounterConfig{ 4U, 42UL };
    REQUIRE(a == b);
  }

  SECTION("not equal when type differs")
  {
    const auto a = perf::CounterConfig{ 4U, 42UL };
    const auto b = perf::CounterConfig{ 5U, 42UL };
    REQUIRE_FALSE(a == b);
  }

  SECTION("not equal when config[0] differs")
  {
    const auto a = perf::CounterConfig{ 4U, 42UL };
    const auto b = perf::CounterConfig{ 4U, 99UL };
    REQUIRE_FALSE(a == b);
  }

  SECTION("equal even when config[1] and config[2] differ")
  {
    const auto a = perf::CounterConfig{ 4U, 42UL, 1UL, 2UL };
    const auto b = perf::CounterConfig{ 4U, 42UL, 9UL, 9UL };
    REQUIRE(a == b);
  }

  SECTION("equal even when scale differs")
  {
    auto a = perf::CounterConfig{ 4U, 42UL };
    auto b = perf::CounterConfig{ 4U, 42UL };
    b.scale(99.0);
    REQUIRE(a == b);
  }

  SECTION("equal even when is_fixed differs")
  {
    const auto a = perf::CounterConfig{ 4U, 42UL, 0UL, 0UL, false };
    const auto b = perf::CounterConfig{ 4U, 42UL, 0UL, 0UL, true };
    REQUIRE(a == b);
  }
}
