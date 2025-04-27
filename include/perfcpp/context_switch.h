#pragma once
#include <cstdint>
#include <optional>

namespace perf {
class ContextSwitch
{
public:
  ContextSwitch(const bool is_out,
                const bool is_preempt,
                const std::optional<std::uint32_t> process_id,
                const std::optional<std::uint32_t> thread_id) noexcept
    : _is_out(is_out)
    , _is_preempt(is_preempt)
    , _process_id(process_id)
    , _thread_id(thread_id)
  {
  }
  ~ContextSwitch() noexcept = default;

  /**
   * @return True, if the process/thread was switched out.
   */
  [[nodiscard]] bool is_out() const noexcept { return _is_out; }

  /**
   * @return True, if the process/thread was switched in.
   */
  [[nodiscard]] bool is_in() const noexcept { return !_is_out; }

  /**
   * @return True, if the process/thread was preempted.
   */
  [[nodiscard]] bool is_preempt() const noexcept { return _is_preempt; }

  /**
   * @return Id of the process, or std::nullopt if not provided (currently only provided on CPU-wide sampling).
   */
  [[nodiscard]] std::optional<std::uint32_t> process_id() const noexcept { return _process_id; }

  /**
   * @return Id of the thread, or std::nullopt if not provided (currently only provided on CPU-wide sampling).
   */
  [[nodiscard]] std::optional<std::uint32_t> thread_id() const noexcept { return _thread_id; }

private:
  bool _is_out;
  bool _is_preempt;
  std::optional<std::uint32_t> _process_id{ std::nullopt };
  std::optional<std::uint32_t> _thread_id{ std::nullopt };
};
}
