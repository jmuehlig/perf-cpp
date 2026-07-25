#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <iterator>
#include <perfcpp/analyzer/flame_graph_generator.hpp>
#include <perfcpp/sample/sample.hpp>
#include <perfcpp/util/callchain_trie.hpp>
#include <string>
#include <vector>

namespace {
/// Deliberately bogus, unmapped instruction pointer addresses — see test/callchain_trie.cpp for why
/// this keeps symbol resolution deterministic and independent of the running binary.
constexpr std::uintptr_t ADDR_A = 0x1U;
constexpr std::uintptr_t ADDR_B = 0x2U;
constexpr std::uintptr_t ADDR_C = 0x3U;

/// Resolves the fallback name a fresh CallchainTrie would assign to a single bogus address.
/// Used as an independent oracle to verify FlameGraphGenerator's root-to-leaf ordering, since
/// the fallback name is a deterministic function of the address alone.
std::string
resolve_name(const std::uintptr_t address)
{
  auto trie = perf::util::CallchainTrie{};
  const auto id = trie.insert({ address });
  return trie.node(id).name;
}

/// Builds a sample whose callchain (perf's leaf-to-root caller order, i.e. excluding the sample's
/// own instruction pointer) and logical instruction pointer are set explicitly.
perf::Sample
make_sample(std::vector<std::uintptr_t> callers_leaf_to_root, const std::uintptr_t logical_instruction_pointer)
{
  auto sample = perf::Sample{};
  sample.instruction_execution().callchain(std::move(callers_leaf_to_root));
  sample.instruction_execution().logical_instruction_pointer(logical_instruction_pointer);
  return sample;
}
}

TEST_CASE("map folds a single sample into one root-to-leaf stack", "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  /// Callers, leaf-to-root: B is the nearer caller, A is the root. C is the sample's own
  /// (innermost) instruction pointer, so the resulting stack must read root-to-leaf as A, B, C.
  const auto samples = std::vector<perf::Sample>{ make_sample({ ADDR_B, ADDR_A }, ADDR_C) };
  const auto stacks = generator.map(samples);

  REQUIRE(stacks.size() == 1U);
  REQUIRE(stacks.front().second == 1U);
  REQUIRE(stacks.front().first ==
          std::vector<std::string>{ resolve_name(ADDR_A), resolve_name(ADDR_B), resolve_name(ADDR_C) });
}

TEST_CASE("map folds a sample with no caller frames to a single-element leaf stack", "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  const auto samples = std::vector<perf::Sample>{ make_sample({}, ADDR_A) };
  const auto stacks = generator.map(samples);

  REQUIRE(stacks.size() == 1U);
  REQUIRE(stacks.front().second == 1U);
  REQUIRE(stacks.front().first == std::vector<std::string>{ resolve_name(ADDR_A) });
}

TEST_CASE("map merges identical stacks and accumulates their sample count", "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  const auto samples = std::vector<perf::Sample>{ make_sample({ ADDR_A }, ADDR_B), make_sample({ ADDR_A }, ADDR_B) };
  const auto stacks = generator.map(samples);

  REQUIRE(stacks.size() == 1U);
  REQUIRE(stacks.front().second == 2U);
}

TEST_CASE("map keeps distinct stacks separate", "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  const auto samples = std::vector<perf::Sample>{ make_sample({}, ADDR_A), make_sample({}, ADDR_B) };
  const auto stacks = generator.map(samples);

  REQUIRE(stacks.size() == 2U);
  for (const auto& [path, count] : stacks) {
    REQUIRE(count == 1U);
    REQUIRE(path.size() == 1U);
  }
}

TEST_CASE("map keeps samples that share a prefix but diverge at the leaf as separate stacks", "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  const auto samples = std::vector<perf::Sample>{ make_sample({ ADDR_A }, ADDR_B), make_sample({ ADDR_A }, ADDR_C) };
  const auto stacks = generator.map(samples);

  REQUIRE(stacks.size() == 2U);

  const auto expected_ab = std::vector<std::string>{ resolve_name(ADDR_A), resolve_name(ADDR_B) };
  const auto expected_ac = std::vector<std::string>{ resolve_name(ADDR_A), resolve_name(ADDR_C) };

  auto found_ab = false;
  auto found_ac = false;
  for (const auto& [path, count] : stacks) {
    REQUIRE(count == 1U);
    if (path == expected_ab) {
      found_ab = true;
    } else if (path == expected_ac) {
      found_ac = true;
    }
  }
  REQUIRE(found_ab);
  REQUIRE(found_ac);
}

TEST_CASE("map on an empty sample list returns no stacks", "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  const auto stacks = generator.map(std::vector<perf::Sample>{});
  REQUIRE(stacks.empty());
}

TEST_CASE("map with a custom mapper uses the mapper's weight instead of the raw sample count", "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  const auto samples =
    std::vector<perf::Sample>{ make_sample({}, ADDR_A), make_sample({}, ADDR_A), make_sample({}, ADDR_A) };

  const auto stacks = generator.map(samples,
                                    [](std::vector<perf::Sample>::const_iterator begin,
                                       std::vector<perf::Sample>::const_iterator end) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(std::distance(begin, end)) * 100U;
                                    });

  REQUIRE(stacks.size() == 1U);
  /// The default weight would be the raw sample count (3); a working custom mapper must override it.
  REQUIRE(stacks.front().second == 300U);
}

TEST_CASE("map with a custom mapper invokes it once per distinct stack, over only that stack's samples",
          "[FlameGraphGenerator]")
{
  auto generator = perf::analyzer::FlameGraphGenerator{};

  const auto samples =
    std::vector<perf::Sample>{ make_sample({}, ADDR_A), make_sample({}, ADDR_A), make_sample({}, ADDR_B) };

  const auto stacks = generator.map(samples,
                                    [](std::vector<perf::Sample>::const_iterator begin,
                                       std::vector<perf::Sample>::const_iterator end) -> std::uint64_t {
                                      return static_cast<std::uint64_t>(std::distance(begin, end)) * 100U;
                                    });

  REQUIRE(stacks.size() == 2U);

  const auto expected_a = std::vector<std::string>{ resolve_name(ADDR_A) };
  const auto expected_b = std::vector<std::string>{ resolve_name(ADDR_B) };

  for (const auto& [path, count] : stacks) {
    if (path == expected_a) {
      REQUIRE(count == 200U);
    } else if (path == expected_b) {
      REQUIRE(count == 100U);
    } else {
      FAIL("Unexpected stack in output");
    }
  }
}
