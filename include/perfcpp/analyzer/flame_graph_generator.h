#pragma once
#include <cstdint>
#include <functional>
#include <perfcpp/sample.h>
#include <perfcpp/symbol_resolver.h>
#include <string>
#include <utility>
#include <vector>

namespace perf::analyzer {
class FlameGraphGenerator
{
public:
  FlameGraphGenerator() = default;
  FlameGraphGenerator(const FlameGraphGenerator&) = default;
  FlameGraphGenerator(FlameGraphGenerator&&) noexcept = default;
  ~FlameGraphGenerator() = default;
  FlameGraphGenerator& operator=(const FlameGraphGenerator&) = default;
  FlameGraphGenerator& operator=(FlameGraphGenerator&&) noexcept = default;

  [[nodiscard]] std::vector<std::pair<std::vector<std::string>, std::uint64_t>> map(const std::vector<Sample>& samples);

  [[nodiscard]] std::vector<std::pair<std::vector<std::string>, std::uint64_t>> map(
    const std::vector<Sample>& samples,
    std::function<std::uint64_t(std::vector<Sample>::const_iterator begin, std::vector<Sample>::const_iterator end)>&&
      mapper);

  void map(const std::vector<Sample>& samples, std::string&& out_file_path) { map(samples, out_file_path); }

  void map(const std::vector<Sample>& samples, const std::string& out_file_path);

  void map(const std::vector<Sample>& samples,
           std::function<std::uint64_t(std::vector<Sample>::const_iterator begin,
                                       std::vector<Sample>::const_iterator end)>&& mapper,
           std::string&& out_file_path)
  {
    map(samples, std::move(mapper), out_file_path);
  }

  void map(const std::vector<Sample>& samples,
           std::function<std::uint64_t(std::vector<Sample>::const_iterator begin,
                                       std::vector<Sample>::const_iterator end)> mapper,
           const std::string& out_file_path);

private:
  SymbolResolver _symbol_resolver;

  [[nodiscard]] bool have_equal_call_chains(const Sample& original_sample, const Sample& follow_up_sample) noexcept;

  [[nodiscard]] static bool have_equal_symbols(const std::optional<SymbolResolver::ResolvedSymbol>& first,
                                               const std::optional<SymbolResolver::ResolvedSymbol>& second) noexcept;

  [[nodiscard]] std::vector<std::string> resolve_symbols(const std::optional<std::vector<std::uintptr_t>>& callchain,
                                                         std::optional<std::uintptr_t> top_logical_instruction_pointer);

  [[nodiscard]] static std::string to_hex(std::uintptr_t logical_instruction_pointer);
};
}