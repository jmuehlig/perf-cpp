#pragma once

#include <cstdint>

namespace perf {
/**
 * Contains information about the "weight" of a memory-based sample, whereas weight mostly refers to the latency of a
 * memory instruction.
 *
 * DEPRECATED: The latency structure will be removed from v0.12. Use InstructionExecution::Latency and DataAccess::Latency instead.
 */
class Latency
{
public:
  Latency() noexcept = default;

  Latency(const std::uint32_t cache_latency, const std::uint32_t instruction_retirement_latency) noexcept
    : _instruction_retirement_latency(instruction_retirement_latency)
    , _cache_latency(cache_latency)
  {
  }

  explicit Latency(const std::uint32_t latency) noexcept
    : _instruction_retirement_latency(latency)
  {
  }

  ~Latency() noexcept = default;

  /**
   * @return Latency of the instruction/uop. On Intel, this includes also TLB latency and cache latency. On AMD, this is
   * the latency of the uop, beginning from tagging via IBS, stopping at the uop retirement.
   */
  [[deprecated("Will be removed in v0.12. Use InstructionExecution::Latency instead.")]] [[nodiscard]] std::uint32_t instruction_retirement_latency() const noexcept
  {
    return _instruction_retirement_latency;
  }

  /**
   * @return Latency from cache-access until the memory subsystem returns the data (in CPU core cycles). For stores,
   * this refers to the latency from L1d until data is written to the memory subsystem. The latency does not include TLB
   * lookups. On IBS, this is only the cache miss latency; i.e., the latency for L1d hits is always reported as 0.
   */
  [[deprecated("Will be removed in v0.12. Use DataAccess::Latency instead.")]] [[nodiscard]] std::uint32_t cache_latency() const noexcept { return _cache_latency; }

private:
  std::uint32_t _instruction_retirement_latency{ 0U };
  std::uint32_t _cache_latency{ 0U };
};
}