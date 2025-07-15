#pragma once

#include "expression.h"
#include <memory>
#include <optional>
#include <vector>

namespace perf::metric::expression {
/**
 * Calculates the ratio between both operands.
 */
class DRatioFunction final : public BinaryExpression
{
public:
  DRatioFunction(std::unique_ptr<ExpressionInterface>&& left, std::unique_ptr<ExpressionInterface>&& right)
    : BinaryExpression(std::move(left), std::move(right))
  {
  }

  ~DRatioFunction() override = default;

protected:
  [[nodiscard]] std::optional<double> evaluate(std::optional<double> left, std::optional<double> right) const override;
};

class SumFunction final : public ExpressionInterface
{
public:
  explicit SumFunction(std::vector<std::unique_ptr<ExpressionInterface>>&& arguments)
    : _arguments(std::move(arguments))
  {
  }

  ~SumFunction() override = default;

  /**
   * Sums up all arguments.
   *
   * @param result List of results.
   * @return The sum of all arguments.
   */
  [[nodiscard]] std::optional<double> evaluate(const CounterResult& result) const override;

  /**
   * Adds all counters for all arguments.
   *
   * @param hardware_counter_names List of hardware counters that will be augmented.
   */
  void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const override;

private:
  std::vector<std::unique_ptr<ExpressionInterface>> _arguments;
};
}