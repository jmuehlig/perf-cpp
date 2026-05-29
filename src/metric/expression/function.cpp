#include <perfcpp/metric/expression/function.hpp>

std::optional<double>
perf::metric::expression::DRatioFunction::evaluate(const std::optional<double> left,
                                                   const std::optional<double> right) const
{
  if (!left.has_value() || !right.has_value()) {
    return std::nullopt;
  }

  /// Return 0 when the denominator is zero, matching dratio() semantics.
  if (right.value() == .0) {
    return .0;
  }

  return left.value() / right.value();
}

std::optional<double>
perf::metric::expression::SumFunction::evaluate(const perf::CounterResult& result) const
{
  auto sum = .0;

  for (const auto& argument : this->_arguments) {
    /// Evaluate the argument.
    const auto evaluated_argument = argument->evaluate(result);

    /// If the argument cannot be evaluated, the function fails.
    if (!evaluated_argument.has_value()) {
      return std::nullopt;
    }

    /// Accumulate.
    sum += evaluated_argument.value();
  }

  return sum;
}

void
perf::metric::expression::SumFunction::add_required_hardware_counter(
  std::vector<std::string>& hardware_counter_names) const
{
  for (const auto& argument : this->_arguments) {
    argument->add_required_hardware_counter(hardware_counter_names);
  }
}