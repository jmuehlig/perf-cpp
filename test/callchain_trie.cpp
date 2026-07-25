#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <perfcpp/util/callchain_trie.hpp>
#include <string>
#include <utility>
#include <vector>

namespace {
/// Deliberately bogus, unmapped instruction pointer addresses. Since they cannot resolve to a real
/// symbol via the internal SymbolResolver, they deterministically fall back to a hex-string name
/// (per the class documentation), independent of the running binary's actual symbol table. Distinct
/// addresses always yield distinct fallback names, and identical addresses always yield identical names.
constexpr std::uintptr_t ADDR_A = 0x1U;
constexpr std::uintptr_t ADDR_B = 0x2U;
constexpr std::uintptr_t ADDR_C = 0x3U;
constexpr std::uintptr_t ADDR_X = 0x10U;
constexpr std::uintptr_t ADDR_X1 = 0x11U;
constexpr std::uintptr_t ADDR_Y = 0x20U;
constexpr std::uintptr_t ADDR_Y1 = 0x21U;
}

TEST_CASE("empty trie", "[CallchainTrie]")
{
  const auto trie = perf::util::CallchainTrie{};
  REQUIRE(trie.empty());
  REQUIRE(trie.size() == 0U);
}

TEST_CASE("inserting a single-address callchain creates one root-level leaf", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  const auto leaf_id = trie.insert({ ADDR_A });

  REQUIRE_FALSE(trie.empty());
  REQUIRE(trie.size() == 1U);
  REQUIRE(trie.node(leaf_id).count == 1U);
  REQUIRE_FALSE(trie.node(leaf_id).parent_id.has_value());
}

TEST_CASE("inserting the same callchain twice merges into one node and accumulates weight", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  const auto first_leaf_id = trie.insert({ ADDR_A });
  const auto second_leaf_id = trie.insert({ ADDR_A });

  /// No new node was created for the duplicate insertion.
  REQUIRE(trie.size() == 1U);
  REQUIRE(second_leaf_id == first_leaf_id);

  /// The leaf's sample count accumulates across both insertions.
  REQUIRE(trie.node(first_leaf_id).count == 2U);
}

TEST_CASE("inserting a callchain five times accumulates a count of five", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  auto leaf_id = perf::util::CallchainTrie::node_id_t{};
  for (auto i = 0U; i < 5U; ++i) {
    leaf_id = trie.insert({ ADDR_A, ADDR_B });
  }

  REQUIRE(trie.size() == 2U);
  REQUIRE(trie.node(leaf_id).count == 5U);
}

TEST_CASE("inserting two distinct single-address callchains creates two independent root-level nodes",
          "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  const auto leaf_a = trie.insert({ ADDR_A });
  const auto leaf_b = trie.insert({ ADDR_B });

  REQUIRE(trie.size() == 2U);
  REQUIRE(leaf_a != leaf_b);
  REQUIRE_FALSE(trie.node(leaf_a).parent_id.has_value());
  REQUIRE_FALSE(trie.node(leaf_b).parent_id.has_value());
  REQUIRE(trie.node(leaf_a).name != trie.node(leaf_b).name);
}

TEST_CASE("extending a previously-inserted prefix reuses its node instead of duplicating it", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  const auto leaf_a = trie.insert({ ADDR_A });
  const auto leaf_ab = trie.insert({ ADDR_A, ADDR_B });

  /// Only one new node (B) was created; A was reused as the shared prefix.
  REQUIRE(trie.size() == 2U);

  /// The earlier, unrelated insertion of the standalone "A" leaf is unaffected by extending it.
  REQUIRE(trie.node(leaf_a).count == 1U);
  REQUIRE(trie.node(leaf_ab).count == 1U);

  /// B's parent is the reused A node.
  REQUIRE(trie.node(leaf_ab).parent_id.has_value());
  REQUIRE(trie.node(leaf_ab).parent_id.value() == leaf_a);
}

TEST_CASE("two callchains sharing a prefix diverge into distinct children", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  const auto leaf_ab = trie.insert({ ADDR_A, ADDR_B });
  const auto leaf_ac = trie.insert({ ADDR_A, ADDR_C });

  /// Three nodes total: the shared A prefix plus the two divergent leaves B and C.
  REQUIRE(trie.size() == 3U);
  REQUIRE(leaf_ab != leaf_ac);

  REQUIRE(trie.node(leaf_ab).parent_id.has_value());
  REQUIRE(trie.node(leaf_ac).parent_id.has_value());
  REQUIRE(trie.node(leaf_ab).parent_id.value() == trie.node(leaf_ac).parent_id.value());
}

TEST_CASE("path returns symbol names from root to the given node", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  const auto leaf_a = trie.insert({ ADDR_A });
  const auto leaf_ab = trie.insert({ ADDR_A, ADDR_B });

  const auto path_to_ab = trie.path(leaf_ab);
  REQUIRE(path_to_ab.size() == 2U);
  REQUIRE(path_to_ab.at(0) == trie.node(leaf_a).name);
  REQUIRE(path_to_ab.at(1) == trie.node(leaf_ab).name);
  REQUIRE(path_to_ab.at(0) != path_to_ab.at(1));

  const auto path_to_a = trie.path(leaf_a);
  REQUIRE(path_to_a.size() == 1U);
  REQUIRE(path_to_a.at(0) == trie.node(leaf_a).name);
}

TEST_CASE("clear empties the trie and lets it be filled again from scratch", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  trie.insert({ ADDR_A, ADDR_B });
  REQUIRE_FALSE(trie.empty());

  trie.clear();
  REQUIRE(trie.empty());
  REQUIRE(trie.size() == 0U);

  const auto leaf_id = trie.insert({ ADDR_C });
  REQUIRE(trie.size() == 1U);
  REQUIRE(trie.node(leaf_id).count == 1U);
}

TEST_CASE("for_each_stack visits only nodes with a non-zero sample count", "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  /// Only the leaf (A, B) receives a count; the intermediate A prefix node stays at count zero
  /// because it was never itself the terminal node of an insertion.
  trie.insert({ ADDR_A, ADDR_B });

  auto visited = std::vector<std::pair<std::vector<std::string>, std::uint64_t>>{};
  trie.for_each_stack([&visited](const std::vector<std::string>& stack, const std::uint64_t count) {
    visited.emplace_back(stack, count);
  });

  REQUIRE(visited.size() == 1U);
  REQUIRE(visited.front().first.size() == 2U);
  REQUIRE(visited.front().second == 1U);
}

TEST_CASE("for_each_stack visits an intermediate node that was also independently inserted as a leaf",
          "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  const auto leaf_a = trie.insert({ ADDR_A });
  trie.insert({ ADDR_A, ADDR_B });

  auto visited = std::vector<std::pair<std::vector<std::string>, std::uint64_t>>{};
  trie.for_each_stack([&visited](const std::vector<std::string>& stack, const std::uint64_t count) {
    visited.emplace_back(stack, count);
  });

  REQUIRE(visited.size() == 2U);

  auto found_a = false;
  auto found_ab = false;
  for (const auto& [stack, count] : visited) {
    if (stack.size() == 1U) {
      found_a = true;
      REQUIRE(count == 1U);
      REQUIRE(stack.front() == trie.node(leaf_a).name);
    } else if (stack.size() == 2U) {
      found_ab = true;
      REQUIRE(count == 1U);
    }
  }

  REQUIRE(found_a);
  REQUIRE(found_ab);
}

TEST_CASE("for_each_stack performs a depth-first traversal that does not interleave sibling subtrees",
          "[CallchainTrie]")
{
  auto trie = perf::util::CallchainTrie{};
  /// Two independent root-level branches, each two levels deep.
  const auto leaf_x = trie.insert({ ADDR_X });
  trie.insert({ ADDR_X, ADDR_X1 });
  trie.insert({ ADDR_Y });
  trie.insert({ ADDR_Y, ADDR_Y1 });

  /// Captured up front so the traversal callback below never mutates the trie it is iterating over.
  const auto name_x = trie.node(leaf_x).name;

  auto visited_branches = std::vector<bool /* is_x_branch */>{};
  trie.for_each_stack([&](const std::vector<std::string>& stack, std::uint64_t /*count*/) {
    visited_branches.push_back(stack.front() == name_x);
  });

  REQUIRE(visited_branches.size() == 4U);

  /// A depth-first traversal must fully visit one branch's nodes before moving to the sibling
  /// branch; entries belonging to the same branch must therefore be contiguous.
  auto current_branch = visited_branches.front();
  auto switched_branch_count = 0U;
  for (const auto is_x_branch : visited_branches) {
    if (is_x_branch != current_branch) {
      current_branch = is_x_branch;
      ++switched_branch_count;
    }
  }
  REQUIRE(switched_branch_count == 1U);
}
