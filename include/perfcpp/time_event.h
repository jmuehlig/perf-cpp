#pragma once
#include <chrono>
#include <string>

namespace perf {
class TimeEvent
{
public:
  virtual ~TimeEvent() noexcept = default;
  [[nodiscard]] virtual std::uint64_t calculate(std::chrono::steady_clock::time_point start,
                                                std::chrono::steady_clock::time_point end) const noexcept = 0;
};

class SecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] std::uint64_t calculate(const std::chrono::steady_clock::time_point start,
                                                const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::seconds>(end - start).count());
  }
};

class MillisecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] std::uint64_t calculate(const std::chrono::steady_clock::time_point start,
                                                const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
  }
};

class MicrosecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] std::uint64_t calculate(const std::chrono::steady_clock::time_point start,
                                                const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::microseconds>(end - start).count());
  }
};

class NanosecondsTimeEvent final : public TimeEvent
{
public:
  [[nodiscard]] std::uint64_t calculate(const std::chrono::steady_clock::time_point start,
                                                const std::chrono::steady_clock::time_point end) const noexcept override
  {
    return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
  }
};
}