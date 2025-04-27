#pragma once

namespace perf {
class Throttle
{
public:
  explicit Throttle(const bool is_throttle) noexcept
    : _is_throttle(is_throttle)
  {
  }
  ~Throttle() noexcept = default;

  /**
   * @return True, if the event was a throttle event.
   */
  [[nodiscard]] bool is_throttle() const noexcept { return _is_throttle; }

  /**
   * @return True, if the event was an unthrottle event.
   */
  [[nodiscard]] bool is_unthrottle() const noexcept { return !_is_throttle; }

private:
  bool _is_throttle;
};
}