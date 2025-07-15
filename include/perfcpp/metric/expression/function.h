#pragma once

#include "expression.h"

namespace perf::metric::expression {
class DRatioFunction final : public ExpressionInterface
{
public:
  DRatioFunction(std::unique_ptr<ExpressionInterface>&& left, std::unique_ptr<ExpressionInterface>&& right)
    : _left(std::move(left))
    , _right(std::move(right))
  {
  }

  ~DRatioFunction() override = default;

  /**
   * Evaluates the d_ratio function recursively, i.e., all sub expressions are also evaluated.
   *
   * @param result List of results.
   * @return The evaluated d_ratio, if all required counters are available in the result.
   */
  [[nodiscard]] std::optional<double> evaluate(const CounterResult& result) const override;

  /**
   * Adds all counters for both operands of the d_ratio function.
   *
   * @param hardware_counter_names List of hardware counters that will be augmented.
   */
  void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const override
  {
    _left->add_required_hardware_counter(hardware_counter_names);
    _right->add_required_hardware_counter(hardware_counter_names);
  }

private:
  std::unique_ptr<ExpressionInterface> _left;
  std::unique_ptr<ExpressionInterface> _right;
};
}