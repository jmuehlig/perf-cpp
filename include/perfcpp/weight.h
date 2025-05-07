#pragma once

#include <cstdint>

namespace perf {
/**
 * Contains information about the "weight" of a memory-based sample, whereas weight mostly refers to the latency of a
 * memory instruction.
 *
 * DEPRECATED: The weight structure will be removed from v0.12. Use InstructionExecution::Latency and
 * DataAccess::Latency instead.
 */
class Weight
{
public:
  Weight() noexcept = default;

  Weight(const std::uint32_t cache_latency,
         const std::uint32_t instruction_retirement_latency,
         const std::uint16_t var3_w) noexcept
    : _var1(cache_latency)
    , _var2(instruction_retirement_latency)
    , _var3(var3_w)
  {
  }

  explicit Weight(const std::uint32_t latency) noexcept
    : _var1(latency)
  {
  }

  ~Weight() noexcept = default;

  /**
   * @return Latency from cache-access until the memory subsystem returns the data (in CPU core cycles). For stores,
   * this refers to the latency from L1d until data is written to the memory subsystem. The latency does not include TLB
   * lookups.
   */
  [[deprecated("Will be removed in v0.12. Use DataAccess::Latency instead.")]] [[nodiscard]] std::uint32_t
  cache_latency() const noexcept
  {
    return _var1;
  }

  /**
   * @return The latency from dispatch until retirement of a load instruction, including TLB lookups.
   */
  [[deprecated("Will be removed in v0.12. Use InstructionExecution::Latency instead.")]] [[nodiscard]] std::uint32_t
  instruction_retirement_latency() const noexcept
  {
    return _var2;
  }

  [[deprecated(
    "Will be removed in v0.12. Use InstructionExecution::Latency or DataAccess::Latency instead.")]] [[nodiscard]] std::
    uint32_t
    latency() const noexcept
  {
    return _var1;
  }

  [[deprecated(
    "Will be removed in v0.12. Use InstructionExecution::Latency or DataAccess::Latency instead.")]] [[nodiscard]] std::
    uint32_t
    var1() const noexcept
  {
    return _var1;
  }
  [[deprecated(
    "Will be removed in v0.12. Use InstructionExecution::Latency or DataAccess::Latency instead.")]] [[nodiscard]] std::
    uint32_t
    var2() const noexcept
  {
    return _var2;
  }
  [[deprecated(
    "Will be removed in v0.12. Use InstructionExecution::Latency or DataAccess::Latency instead.")]] [[nodiscard]] std::
    uint32_t
    var3() const noexcept
  {
    return _var3;
  }

private:
  std::uint32_t _var1{ 0U };
  std::uint32_t _var2{ 0U };
  std::uint16_t _var3{ 0U };
};
}