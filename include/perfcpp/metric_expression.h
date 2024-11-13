#pragma once

#include "counter.h"
#include "exception.h"
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace perf {

enum class Operator
{
  Plus,
  Minus,
  Times,
  Divide
};

class MetricExpression
{
public:
  MetricExpression() noexcept = default;
  virtual ~MetricExpression() = default;
  [[nodiscard]] virtual std::optional<double> evaluate(const CounterResult& result) const = 0;
  virtual void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const = 0;
};

class ConstantExpression final : public MetricExpression
{
public:
  explicit ConstantExpression(const double value) noexcept
    : _value(value)
  {
  }

  ~ConstantExpression() noexcept override = default;

  [[nodiscard]] std::optional<double> evaluate(const CounterResult& /* result */) const override { return _value; }

  void add_required_hardware_counter(std::vector<std::string>& /* hardware_counter_names */) const override {}

private:
  const double _value;
};

class IdentifierExpression final : public MetricExpression
{
public:
  explicit IdentifierExpression(std::string&& identifier)
    : _identifier(std::move(identifier))
  {
  }

  ~IdentifierExpression() override = default;

  [[nodiscard]] std::optional<double> evaluate(const CounterResult& result) const override
  {
    return result.get(_identifier);
  }

  void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const override
  {
    hardware_counter_names.push_back(_identifier);
  }

private:
  const std::string _identifier;
};

template<Operator OP>
class BinaryExpression final : public MetricExpression
{
public:
  BinaryExpression(std::unique_ptr<MetricExpression>&& left, std::unique_ptr<MetricExpression>&& right)
    : _left(std::move(left))
    , _right(std::move(right))
  {
  }

  ~BinaryExpression() override = default;

  [[nodiscard]] std::optional<double> evaluate(const CounterResult& result) const override
  {
    if (const auto left = this->_left->evaluate(result); left.has_value()) {
      if (const auto right = this->_right->evaluate(result); right.has_value()) {

        if constexpr (OP == Operator::Plus) {
          return left.value() + right.value();
        }

        if constexpr (OP == Operator::Minus) {
          return left.value() - right.value();
        }

        if constexpr (OP == Operator::Times) {
          return left.value() * right.value();
        }

        if constexpr (OP == Operator::Divide) {
          if (right.value() > .0) {
            return left.value() / right.value();
          }
        }
      }
    }

    return std::nullopt;
  }

  void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const override
  {
    _left->add_required_hardware_counter(hardware_counter_names);
    _right->add_required_hardware_counter(hardware_counter_names);
  }

private:
  std::unique_ptr<MetricExpression> _left;
  std::unique_ptr<MetricExpression> _right;
};

class Token
{
public:
  enum class Type
  {
    ConstantNumber,
    Identifier,
    Operator,
    LeftParenthesis,
    RightParenthesis,
    End
  };

  Token() noexcept = default;

  explicit Token(const Operator operator_)
    : _type(Type::Operator)
    , _operator(operator_)
  {
  }
  explicit Token(const Type type)
    : _type(type)
  {
  }
  explicit Token(const double number)
    : _type(Type::ConstantNumber)
    , _number(number)
  {
  }
  explicit Token(std::string&& text)
    : _type(Type::Identifier)
    , _text(std::move(text))
  {
  }
  ~Token() = default;

  bool operator==(const Type type) const noexcept { return _type == type; }

  [[nodiscard]] Type type() const noexcept { return _type; }
  [[nodiscard]] const std::optional<std::string>& text() const noexcept { return _text; }
  [[nodiscard]] std::optional<std::string>& text() noexcept { return _text; }
  [[nodiscard]] std::optional<double> number() const noexcept { return _number; }
  [[nodiscard]] std::optional<Operator> operator_() const noexcept { return _operator; }

  [[nodiscard]] bool is_dot_operation() const noexcept
  {
    return _type == Type::Operator && _operator.has_value() &&
           (_operator.value() == Operator::Times || _operator.value() == Operator::Divide);
  }

  [[nodiscard]] bool is_dash_operation() const noexcept
  {
    return _type == Type::Operator && _operator.has_value() &&
           (_operator.value() == Operator::Plus || _operator.value() == Operator::Minus);
  }

private:
  Type _type{ Type::End };
  std::optional<std::string> _text{ std::nullopt };
  std::optional<double> _number{ std::nullopt };
  std::optional<Operator> _operator{ std::nullopt };
};

class Tokenizer
{
public:
  explicit Tokenizer(std::string&& input)
    : _input(std::move(input))
  {
  }

  [[nodiscard]] const std::string& input() const noexcept { return _input; }

  [[nodiscard]] Token next();

private:
  [[nodiscard]] Token read_constant_number();
  [[nodiscard]] Token read_identifier();

  const std::string _input;
  std::size_t _position{ 0ULL };
};

class Parser
{
public:
  explicit Parser(Tokenizer& tokenizer)
    : _tokenizer(tokenizer)
  {
  }

  [[nodiscard]] std::unique_ptr<MetricExpression> parse() { return parse_dash_operation(); }

private:
  Tokenizer& _tokenizer;

  [[nodiscard]] std::unique_ptr<MetricExpression> parse_dash_operation();
  [[nodiscard]] std::unique_ptr<MetricExpression> parse_dot_expression();
  [[nodiscard]] std::unique_ptr<MetricExpression> parse_factor();
};
}