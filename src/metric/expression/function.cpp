#include <perfcpp/metric/expression/function.h>

std::optional<double>
perf::metric::expression::DRatioFunction::evaluate(const perf::CounterResult& result) const
{
  if (const auto left = this->_left->evaluate(result); left.has_value()) {
    if (const auto right = this->_right->evaluate(result); right.has_value() && right.value() != .0) {
      return left.value() / right.value();
    }
  }

  /// If one of the operands cannot be evaluated OR the right operand is zero, we cannot calculate the ratio.
  return std::nullopt;
}