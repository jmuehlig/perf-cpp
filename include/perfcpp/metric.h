#pragma once

#include "counter.h"
#include <string>
#include <vector>

namespace perf {
class Metric
{
public:
  virtual ~Metric() = default;
  [[nodiscard]] virtual std::string name() const = 0;
  [[nodiscard]] virtual std::vector<std::string> required_counter_names() const = 0;
  [[nodiscard]] virtual std::optional<double> calculate(const CounterResult& result) const = 0;
};

class CyclesPerInstruction final : public Metric
{
public:
  [[nodiscard]] std::string name() const override { return "cycles-per-instruction"; }
  [[nodiscard]] std::vector<std::string> required_counter_names() const override
  {
    return { "cycles", "instructions" };
  }
  [[nodiscard]] std::optional<double> calculate(const CounterResult& result) const override
  {
    const auto cycles = result.get("cycles");
    const auto instructions = result.get("instructions");

    if (cycles.has_value() && instructions.has_value()) {
      return cycles.value() / instructions.value();
    }

    return std::nullopt;
  }
};

class CacheHitRatio final : public Metric
{
public:
  [[nodiscard]] std::string name() const override { return "cache-hit-ratio"; }
  [[nodiscard]] std::vector<std::string> required_counter_names() const override
  {
    return { "cache-misses", "cache-references" };
  }
  [[nodiscard]] std::optional<double> calculate(const CounterResult& result) const override
  {
    const auto misses = result.get("cache-misses");
    const auto references = result.get("cache-references");

    if (misses.has_value() && references.has_value()) {
      return references.value() / misses.value();
    }

    return std::nullopt;
  }
};

class DTLBMissRatio final : public Metric
{
public:
  [[nodiscard]] std::string name() const override { return "dTLB-miss-ratio"; }
  [[nodiscard]] std::vector<std::string> required_counter_names() const override
  {
    return { "dTLB-loads", "dTLB-load-misses" };
  }
  [[nodiscard]] std::optional<double> calculate(const CounterResult& result) const override
  {
    const auto loads = result.get("dTLB-loads");
    const auto misses = result.get("dTLB-load-misses");

    if (loads.has_value() && misses.has_value()) {
      return misses.value() / loads.value();
    }

    return std::nullopt;
  }
};

class ITLBMissRatio final : public Metric
{
public:
  [[nodiscard]] std::string name() const override { return "iTLB-miss-ratio"; }
  [[nodiscard]] std::vector<std::string> required_counter_names() const override
  {
    return { "iTLB-loads", "iTLB-load-misses" };
  }
  [[nodiscard]] std::optional<double> calculate(const CounterResult& result) const override
  {
    const auto loads = result.get("iTLB-loads");
    const auto misses = result.get("iTLB-load-misses");

    if (loads.has_value() && misses.has_value()) {
      return misses.value() / loads.value();
    }

    return std::nullopt;
  }
};

class L1DataMissRatio final : public Metric
{
public:
  [[nodiscard]] std::string name() const override { return "L1-data-miss-ratio"; }
  [[nodiscard]] std::vector<std::string> required_counter_names() const override
  {
    return { "L1-dcache-loads", "L1-dcache-load-misses" };
  }
  [[nodiscard]] std::optional<double> calculate(const CounterResult& result) const override
  {
    const auto loads = result.get("L1-dcache-loads");
    const auto misses = result.get("L1-dcache-load-misses");

    if (loads.has_value() && misses.has_value()) {
      return misses.value() / loads.value();
    }

    return std::nullopt;
  }
};
}