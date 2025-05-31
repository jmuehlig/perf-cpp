#pragma once
#include <cstdint>
#include <functional>
#include <perfcpp/sample.h>
#include <perfcpp/symbol_resolver.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace perf::analyzer {
class FlameGraphGenerator
{
public:
  FlameGraphGenerator() = default;
  ~FlameGraphGenerator() = default;

  [[nodiscard]] std::vector<std::pair<std::vector<std::string>, std::uint64_t>> map(
    const std::vector<Sample>& samples) const;

  [[nodiscard]] std::vector<std::pair<std::vector<std::string>, std::uint64_t>> map(
    const std::vector<Sample>& samples,
    std::function<std::uint64_t(std::vector<Sample>::const_iterator begin, std::vector<Sample>::const_iterator end)>
      mapper) const;

  void map(const std::vector<Sample>& samples, std::string&& out_file_path) const
  {
    return map(samples, out_file_path);
  }

  void map(const std::vector<Sample>& samples, const std::string& out_file_path) const;

  void map(const std::vector<Sample>& samples,
           std::function<std::uint64_t(std::vector<Sample>::const_iterator begin,
                                       std::vector<Sample>::const_iterator end)> mapper,
           std::string&& out_file_path) const
  {
    map(samples, std::move(mapper), out_file_path);
  }

  void map(const std::vector<Sample>& samples,
           std::function<std::uint64_t(std::vector<Sample>::const_iterator begin,
                                       std::vector<Sample>::const_iterator end)> mapper,
           const std::string& out_file_path) const;

private:
  class SymbolCache
  {
  public:
    explicit SymbolCache(const SymbolResolver& symbol_resolver) noexcept
      : _symbol_resolver(symbol_resolver)
    {
    }
    ~SymbolCache() = default;

    [[nodiscard]] std::optional<std::reference_wrapper<const SymbolResolver::Symbol>> symbol(
      std::uintptr_t logical_instruction_pointer);

  private:
    const SymbolResolver _symbol_resolver;
    std::unordered_map<std::uintptr_t, std::optional<std::reference_wrapper<const SymbolResolver::Symbol>>> _cache;
  };

  SymbolResolver _symbol_resolver;

  [[nodiscard]] static bool have_equal_callchains(SymbolCache& symbol_cache,
                                                  const Sample& original_sample,
                                                  const Sample& follow_up_sample) noexcept;

  [[nodiscard]] static bool have_equal_symbols(
    std::optional<std::reference_wrapper<const SymbolResolver::Symbol>> first,
    std::optional<std::reference_wrapper<const SymbolResolver::Symbol>> second) noexcept;

  [[nodiscard]] std::vector<std::string> resolve_symbols(
    const std::optional<std::vector<std::uintptr_t>>& callchain,
    std::optional<std::uintptr_t> top_logical_instruction_pointer) const;
};
}