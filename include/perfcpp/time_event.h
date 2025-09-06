#pragma once
#include <chrono>
#include <string>

namespace perf {
class TimeEvent
{
public:
  virtual ~TimeEvent() noexcept = default;
  [[nodiscard]] virtual double calculate(std::chrono::steady_clock::time_point start,
                                         std::chrono::steady_clock::time_point end) const noexcept = 0;
};

class SecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] double calculate(const std::chrono::steady_clock::time_point start,
                                 const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) / 1000000000.;
  }
};

class MillisecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] double calculate(const std::chrono::steady_clock::time_point start,
                                 const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) / 1000000.;
  }
};

class MicrosecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] double calculate(const std::chrono::steady_clock::time_point start,
                                 const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) / 1000.;
  }
};

class NanosecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] double calculate(const std::chrono::steady_clock::time_point start,
                                 const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  }
};
}