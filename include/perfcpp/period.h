#pragma once
#include <cstdint>
#include <variant>

namespace perf {
class Period
{
public:
  explicit Period(const std::uint64_t period) noexcept
    : _period(period)
  {
  }
  ~Period() noexcept = default;

  [[nodiscard]] std::uint64_t get() const noexcept { return _period; }

private:
  std::uint64_t _period;
};

class Frequency
{
public:
  explicit Frequency(const std::uint64_t frequency) noexcept
    : _frequency(frequency)
  {
  }
  ~Frequency() noexcept = default;

  [[nodiscard]] std::uint64_t get() const noexcept { return _frequency; }

private:
  std::uint64_t _frequency;
};

using PeriodOrFrequency = std::variant<Period, Frequency>;
}