#include <algorithm>
#include <fstream>
#include <iterator>
#include <perfcpp/analyzer/flame_graph_generator.h>

std::optional<std::reference_wrapper<const perf::SymbolResolver::Symbol>>
perf::analyzer::FlameGraphGenerator::SymbolCache::symbol(const std::uintptr_t logical_instruction_pointer)
{
  if (const auto symbol = this->_cache.find(logical_instruction_pointer); symbol != this->_cache.end()) {
    return symbol->second;
  }

  /// Try to resolve the symbol and put also a nullop into the cache, if the symbol wasn't found.
  const auto resolved_symbol = this->_symbol_resolver.resolve(logical_instruction_pointer);
  if (!resolved_symbol.has_value()) {
    this->_cache.insert(std::make_pair(logical_instruction_pointer, std::nullopt));
    return std::nullopt;
  }

  /// Insert the symbol.
  const auto [reference, _] =
    this->_cache.insert(std::make_pair(logical_instruction_pointer, std::ref(resolved_symbol->symbol())));
  return reference->second;
}

std::vector<std::pair<std::vector<std::string>, std::uint64_t>>
perf::analyzer::FlameGraphGenerator::map(const std::vector<Sample>& samples) const
{
  return this->map(samples, [](const auto begin, const auto end) {
    return static_cast<std::uint64_t>(std::distance(begin, end)) + 1U;
  });
}

std::vector<std::pair<std::vector<std::string>, std::uint64_t>>
perf::analyzer::FlameGraphGenerator::map(
  const std::vector<Sample>& samples,
  std::function<std::uint64_t(std::vector<Sample>::const_iterator, std::vector<Sample>::const_iterator)> mapper) const
{
  auto symbol_cache = SymbolCache{ this->_symbol_resolver };

  auto result = std::vector<std::pair<std::vector<std::string>, std::uint64_t>>{};
  result.reserve(samples.size());

  /// Scan samples.
  for (auto iterator = samples.begin(); iterator != samples.end(); ++iterator) {

    /// Find the first sample that does not share the same callchain (plus logical instruction pointer).
    auto next_iterator = std::next(iterator);
    for (; next_iterator != samples.end(); ++next_iterator) {
      if (!FlameGraphGenerator::have_equal_callchains(symbol_cache, *iterator, *next_iterator)) {
        break;
      }
    }

    /// Calculate the weight of the samples.
    const auto weight = mapper(iterator, next_iterator);

    /// Map samples to list of symbols.
    auto callchain = this->resolve_symbols(iterator->instruction_execution().callchain(),
                                           iterator->instruction_execution().logical_instruction_pointer());

    result.emplace_back(std::move(callchain), weight);

    iterator = --next_iterator;
  }

  return result;
}

void
perf::analyzer::FlameGraphGenerator::map(const std::vector<Sample>& samples, const std::string& out_file_path) const
{
  this->map(samples, [](const auto begin, const auto end) { return std::distance(begin, end) + 1U; }, out_file_path);
}

void
perf::analyzer::FlameGraphGenerator::map(
  const std::vector<Sample>& samples,
  std::function<std::uint64_t(std::vector<Sample>::const_iterator, std::vector<Sample>::const_iterator)> mapper,
  const std::string& out_file_path) const
{
  /// Get the stacks.
  const auto stacks = this->map(samples, std::move(mapper));

  auto out_file = std::ofstream{ out_file_path, std::ios_base::trunc };

  /// Write every symbol in the stack to the file.
  for (const auto& [symbols, count] : stacks) {
    for (auto index = 0U; index < symbols.size(); ++index) {
      if (index > 0U) {
        out_file << ';';
      }

      out_file << symbols[index];
    }

    out_file << " " << count << '\n';
  }

  out_file << std::flush;
}

std::vector<std::string>
perf::analyzer::FlameGraphGenerator::resolve_symbols(
  const std::optional<std::vector<std::uintptr_t>>& callchain,
  const std::optional<std::uintptr_t> top_logical_instruction_pointer) const
{
  auto symbol_callchain = std::vector<std::string>{};

  if (callchain.has_value()) {
    symbol_callchain.reserve(callchain->size() + 1U);

    /// Turn the list of instruction pointers from the call stack in symbols (or "??" if the symbol wasn't found).
    std::transform(callchain->begin(),
                   callchain->end(),
                   std::back_inserter(symbol_callchain),
                   [&resolver = this->_symbol_resolver](const auto logical_instruction_pointer) {
                     if (const auto symbol = resolver.resolve(logical_instruction_pointer); symbol.has_value()) {
                       return symbol->symbol().name();
                     }

                     return std::string{ "??" };
                   });
  }

  std::reverse(symbol_callchain.begin(), symbol_callchain.end());

  if (top_logical_instruction_pointer.has_value()) {
    if (const auto symbol = this->_symbol_resolver.resolve(top_logical_instruction_pointer.value());
        symbol.has_value()) {
      symbol_callchain.push_back(symbol->symbol().name());
    } else {
      symbol_callchain.emplace_back("??");
    }
  } else {
    symbol_callchain.emplace_back("??");
  }

  return symbol_callchain;
}

bool
perf::analyzer::FlameGraphGenerator::have_equal_callchains(SymbolCache& symbol_cache,
                                                           const perf::Sample& original_sample,
                                                           const perf::Sample& follow_up_sample) noexcept
{
  if (original_sample.instruction_execution().logical_instruction_pointer().has_value() &&
      !follow_up_sample.instruction_execution().logical_instruction_pointer().has_value()) {
    return false;
  }

  if (!original_sample.instruction_execution().logical_instruction_pointer().has_value() &&
      follow_up_sample.instruction_execution().logical_instruction_pointer().has_value()) {
    return false;
  }

  if (original_sample.instruction_execution().callchain().has_value() &&
      !follow_up_sample.instruction_execution().callchain().has_value()) {
    return false;
  }

  if (!original_sample.instruction_execution().callchain().has_value() &&
      follow_up_sample.instruction_execution().callchain().has_value()) {
    return false;
  }

  /// Check if the symbols are equal, if both have a instruction pointer.
  if (original_sample.instruction_execution().logical_instruction_pointer().has_value() &&
      follow_up_sample.instruction_execution().logical_instruction_pointer().has_value()) {
    const auto original_symbol =
      symbol_cache.symbol(original_sample.instruction_execution().logical_instruction_pointer().value());
    const auto follow_up_symbol =
      symbol_cache.symbol(follow_up_sample.instruction_execution().logical_instruction_pointer().value());
    if (!FlameGraphGenerator::have_equal_symbols(original_symbol, follow_up_symbol)) {
      return false;
    }
  }

  /// Check if both have the same callchain.
  if (original_sample.instruction_execution().callchain().has_value() &&
      follow_up_sample.instruction_execution().callchain().has_value()) {
    const auto& original_callchain = original_sample.instruction_execution().callchain().value();
    const auto& follow_up_callchain = follow_up_sample.instruction_execution().callchain().value();
    if (original_callchain.size() != follow_up_callchain.size()) {
      return false;
    }

    /// Compare entire callchain by resolving the symbols.
    for (auto index = 0U; index < original_callchain.size(); ++index) {
      const auto original_symbol = symbol_cache.symbol(original_callchain[index]);
      const auto follow_up_symbol = symbol_cache.symbol(follow_up_callchain[index]);
      if (!FlameGraphGenerator::have_equal_symbols(original_symbol, follow_up_symbol)) {
        return false;
      }
    }
  }

  return true;
}

bool
perf::analyzer::FlameGraphGenerator::have_equal_symbols(
  std::optional<std::reference_wrapper<const SymbolResolver::Symbol>> first,
  std::optional<std::reference_wrapper<const SymbolResolver::Symbol>> second) noexcept
{
  if (first.has_value() && !second.has_value()) {
    return false;
  }

  if (!first.has_value() && second.has_value()) {
    return false;
  }

  if (!first.has_value() && !second.has_value()) {
    return true;
  }

  return first.value().get() == second.value().get();
}
