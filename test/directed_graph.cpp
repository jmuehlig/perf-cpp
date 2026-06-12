#include <catch2/catch_test_macros.hpp>
#include <perfcpp/util/graph.hpp>
#include <string>
#include <unordered_set>
#include <vector>

TEST_CASE("empty graph", "[DirectedGraph]")
{
  const auto graph = perf::util::DirectedGraph<std::string>{};
  REQUIRE(graph.empty());
}

TEST_CASE("insert makes graph non-empty", "[DirectedGraph]")
{
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.insert("a");
  REQUIRE_FALSE(graph.empty());
}

TEST_CASE("connect without prior insert auto-inserts both nodes", "[DirectedGraph]")
{
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  REQUIRE_FALSE(graph.empty());
}

TEST_CASE("pop on empty graph returns nullopt", "[DirectedGraph]")
{
  auto graph = perf::util::DirectedGraph<std::string>{};
  REQUIRE_FALSE(graph.pop().has_value());
}

TEST_CASE("pop returns the single inserted node", "[DirectedGraph]")
{
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.insert("a");
  REQUIRE_FALSE(graph.empty());

  const auto result = graph.pop();
  REQUIRE(result.has_value());
  REQUIRE(result.value() == "a");

  /// Graph must be empty after the only node is popped.
  REQUIRE_FALSE(graph.pop().has_value());
  REQUIRE(graph.empty());
}

TEST_CASE("pop returns source node in a linear chain", "[DirectedGraph]")
{
  /// a → b → c  — only 'a' has no incoming edge on the first pop.
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("b", "c");

  const auto first = graph.pop();
  REQUIRE(first.has_value());
  REQUIRE(first.value() == "a");

  const auto second = graph.pop();
  REQUIRE(second.has_value());
  REQUIRE(second.value() == "b");

  const auto third = graph.pop();
  REQUIRE(third.has_value());
  REQUIRE(third.value() == "c");

  REQUIRE_FALSE(graph.pop().has_value());
}

TEST_CASE("pop drains a diamond graph in topological order", "[DirectedGraph]")
{
  /// a → b, a → c, b → d, c → d
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("a", "c");
  graph.connect("b", "d");
  graph.connect("c", "d");

  /// 'a' has no incoming edges and must come first.
  const auto first = graph.pop();
  REQUIRE(first.has_value());
  REQUIRE(first.value() == "a");

  /// 'b' and 'c' are now both sources; order between them is unspecified.
  const auto second = graph.pop();
  REQUIRE(second.has_value());
  REQUIRE((second.value() == "b" || second.value() == "c"));

  const auto third = graph.pop();
  REQUIRE(third.has_value());
  REQUIRE((third.value() == "b" || third.value() == "c"));
  REQUIRE(second.value() != third.value());

  /// 'd' is last.
  const auto fourth = graph.pop();
  REQUIRE(fourth.has_value());
  REQUIRE(fourth.value() == "d");

  REQUIRE_FALSE(graph.pop().has_value());
}

TEST_CASE("is_cyclic on empty graph", "[DirectedGraph]")
{
  const auto graph = perf::util::DirectedGraph<std::string>{};
  REQUIRE_FALSE(graph.is_cyclic());
}

TEST_CASE("is_cyclic on single isolated node", "[DirectedGraph]")
{
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.insert("a");
  REQUIRE_FALSE(graph.is_cyclic());
}

TEST_CASE("is_cyclic on linear chain", "[DirectedGraph]")
{
  /// a → b → c — no cycle.
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("b", "c");
  REQUIRE_FALSE(graph.is_cyclic());
}

TEST_CASE("is_cyclic on diamond", "[DirectedGraph]")
{
  /// a → b, a → c, b → d, c → d — shared node, but no cycle.
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("a", "c");
  graph.connect("b", "d");
  graph.connect("c", "d");
  REQUIRE_FALSE(graph.is_cyclic());
}

TEST_CASE("is_cyclic on direct 2-node cycle", "[DirectedGraph]")
{
  /// a → b → a
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("b", "a");
  REQUIRE(graph.is_cyclic());
}

TEST_CASE("is_cyclic on longer cycle", "[DirectedGraph]")
{
  /// a → b → c → a
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("b", "c");
  graph.connect("c", "a");
  REQUIRE(graph.is_cyclic());
}

TEST_CASE("is_cyclic on self-loop", "[DirectedGraph]")
{
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "a");
  REQUIRE(graph.is_cyclic());
}

TEST_CASE("is_cyclic on graph with cycle in a subgraph", "[DirectedGraph]")
{
  /// a → b (acyclic), c → d → c (cycle in separate component)
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("c", "d");
  graph.connect("d", "c");
  REQUIRE(graph.is_cyclic());
}

TEST_CASE("pop returns nullopt on a cyclic graph", "[DirectedGraph]")
{
  /// a → b → a — every node has an incoming edge.
  auto graph = perf::util::DirectedGraph<std::string>{};
  graph.connect("a", "b");
  graph.connect("b", "a");
  REQUIRE_FALSE(graph.pop().has_value());
}
