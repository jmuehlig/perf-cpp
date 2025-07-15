#pragma once

#include "token.h"
#include <cstdint>
#include <memory>
#include <optional>
#include <perfcpp/counter_result.h>
#include <string>
#include <vector>

namespace perf::metric::expression {
/**
 * The interface for all evaluable metric expressions.
 */
class ExpressionInterface
{
public:
  ExpressionInterface() noexcept = default;
  virtual ~ExpressionInterface() = default;

  /**
   * Evaluates the metric with respect to the provided (hardware) counter result.
   *
   * @param result Result of hardware counters.
   * @return The evaluated metric, if all required counters are available.
   */
  [[nodiscard]] virtual std::optional<double> evaluate(const CounterResult& result) const = 0;

  /**
   * Adds names of hardware counters that are needed for this metric.
   * @param hardware_counter_names Names of hardware counters needed to calculate the metric.
   */
  virtual void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const = 0;
};

/**
 * Representation of a constant in a metric expression.
 */
class ConstantExpression final : public ExpressionInterface
{
public:
  explicit ConstantExpression(const double value) noexcept
    : _value(value)
  {
  }

  ~ConstantExpression() noexcept override = default;

  /**
   * @return The constant.
   */
  [[nodiscard]] std::optional<double> evaluate(const CounterResult& /* result */) const override { return _value; }

  /**
   * For constant expressions, no further hardware counters are required.
   */
  void add_required_hardware_counter(std::vector<std::string>& /* hardware_counter_names */) const override {}

private:
  const double _value;
};

/**
 * Resolves an identifier and returns the hardware/time event value of that identifier.
 */
class IdentifierExpression final : public ExpressionInterface
{
public:
  explicit IdentifierExpression(std::string&& identifier)
    : _identifier(std::move(identifier))
  {
  }

  ~IdentifierExpression() override = default;

  /**
   * An identifier refers to a specific counter or metric, i.e., evaluate returns the value of that counter or metric.
   *
   * @param result Result of hardware counters.
   * @return The value of the counter/metric specified by the identifier.
   */
  [[nodiscard]] std::optional<double> evaluate(const CounterResult& result) const override
  {
    return result.get(_identifier);
  }

  /**
   * Adds the identifier as required counter.
   * @param hardware_counter_names List of required counters that will be augmented with the identifier.
   */
  void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const override
  {
    hardware_counter_names.push_back(_identifier);
  }

private:
  const std::string _identifier;
};

/**
 * Implementation of binary expressions (+,-,*,/).
 */
template<Operator_ OP>
class BinaryExpression final : public ExpressionInterface
{
public:
  BinaryExpression(std::unique_ptr<ExpressionInterface>&& left, std::unique_ptr<ExpressionInterface>&& right)
    : _left(std::move(left))
    , _right(std::move(right))
  {
  }

  ~BinaryExpression() override = default;

  /**
   * Evaluates a binary expression recursively, i.e., all sub expressions are also evaluated.
   *
   * @param result List of results.
   * @return The evaluated metric, if all required counters are available in the result.
   */
  [[nodiscard]] std::optional<double> evaluate(const CounterResult& result) const override
  {
    if (const auto left = this->_left->evaluate(result); left.has_value()) {
      if (const auto right = this->_right->evaluate(result); right.has_value()) {

        if constexpr (OP == Operator_::Plus) {
          return left.value() + right.value();
        }

        if constexpr (OP == Operator_::Minus) {
          return left.value() - right.value();
        }

        if constexpr (OP == Operator_::Times) {
          return left.value() * right.value();
        }

        if constexpr (OP == Operator_::Divide) {
          if (right.value() > .0) {
            return left.value() / right.value();
          }
        }
      }
    }

    return std::nullopt;
  }

  /**
   * Adds all counters for both branches of the binary expression.
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