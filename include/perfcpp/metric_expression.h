#pragma once

#include "counter.h"
#include "exception.h"
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
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

/**
 * The interface vor all evaluable metric expressions.
 */
class MetricExpressionInterface
{
public:
  MetricExpressionInterface() noexcept = default;
  virtual ~MetricExpressionInterface() = default;
  [[nodiscard]] virtual std::optional<double> evaluate(const CounterResult& result) const = 0;
  virtual void add_required_hardware_counter(std::vector<std::string>& hardware_counter_names) const = 0;
};

/**
 * Representation of a constant in a metric expression.
 */
class ConstantExpression final : public MetricExpressionInterface
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

/**
 * Resolves an identifier and returns the hardware/time event value of that identifier.
 */
class IdentifierExpression final : public MetricExpressionInterface
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

/**
 * Implementation of binary expressions (+,-,*,/).
 */
template<Operator OP>
class BinaryExpression final : public MetricExpressionInterface
{
public:
  BinaryExpression(std::unique_ptr<MetricExpressionInterface>&& left, std::unique_ptr<MetricExpressionInterface>&& right)
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
  std::unique_ptr<MetricExpressionInterface> _left;
  std::unique_ptr<MetricExpressionInterface> _right;
};

class Token
{
public:
  enum class Type : std::uint8_t
  {
    ConstantNumber,
    Identifier,
    Operator,
    LeftParenthesis,
    RightParenthesis,
  };

  Token(Token&&) noexcept = default;
  Token(const Token&) = default;

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
  bool operator!=(const Type type) const noexcept { return _type != type; }

  [[nodiscard]] Type type() const noexcept { return _type; }
  [[nodiscard]] const std::optional<std::string>& text() const noexcept { return _text; }
  [[nodiscard]] std::optional<std::string>& text() noexcept { return _text; }
  [[nodiscard]] std::optional<double> number() const noexcept { return _number; }
  [[nodiscard]] std::optional<Operator> operator_() const noexcept { return _operator; }

  [[nodiscard]] std::string to_string() const
  {
    switch (_type) {
      case Type::ConstantNumber:
        return std::string{ "constant(" }.append(std::to_string(_number.value())).append(")");
      case Type::Identifier:
        return std::string{ "identifier(" }.append(_text.value()).append(")");
      case Type::Operator:
        switch (_operator.value()) {
          case Operator::Plus:
            return "+";
          case Operator::Minus:
            return "-";
          case Operator::Times:
            return "*";
          case Operator::Divide:
            return "/";
        }
      case Type::LeftParenthesis:
        return "(";
      case Type::RightParenthesis:
        return ")";
    }
  }

private:
  Type _type;
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

  /**
   * @return The original input.
   */
  [[nodiscard]] const std::string& input() const noexcept { return _input; }

  /**
   * Tokenizes the input string.
   * @return Queue of tokens.
   */
  [[nodiscard]] std::queue<Token> tokenize() const;

private:
  /**
   * Reads a constant number (e.g., 13.37) from the input string, starting at the given position.
   *
   * @param position Position within the input string.
   * @return A token containing the constant.
   */
  [[nodiscard]] Token read_constant(std::size_t& position) const;

  /**
   * Reads an identifier (e.g., a hardware counter name) from the input string, starting at the given position.
   *
   * @param position Position within the input string.
   * @return A token containing the identifier.
   */
  [[nodiscard]] Token read_identifier(std::size_t& position) const;

  /**
   * Reads an operator (e.g., +) from the given char.
   *
   * @param current_char Current char from input string.
   * @return A token containing the operator.
   */
  [[nodiscard]] Token read_operator(char current_char) const;

  /**
   * Tests if the left operator has greater precedence than the right operator.
   *
   * @param left_operator Operator.
   * @param right_operator Operator.
   * @return True, if the left operator has a greater precedence than the right operator (or if the right operator is
   * left associative if both have the same precedence).
   */
  [[nodiscard]] static bool has_greater_precedence(const Token& left_operator, const Token& right_operator) noexcept
  {
    const auto left_precedence = precedence(left_operator.operator_().value());
    const auto right_precedence = precedence(right_operator.operator_().value());

    return (is_left_associative(right_operator.operator_().value()) && right_precedence <= left_precedence) ||
           (right_precedence < left_precedence);
  }

  /**
   * Returns the precedence of the given operator.
   *
   * @param operator_ Operator.
   * @return Precedence of the operator.
   */
  [[nodiscard]] static std::uint8_t precedence(const Operator operator_) noexcept
  {
    switch (operator_) {
      case Operator::Plus:
      case Operator::Minus:
        return 4U;
      case Operator::Times:
      case Operator::Divide:
        return 8U;
    }

    return 0U;
  }

  /**
   * Tests if the operator is left associative.
   *
   * @param operator_ Operator to test.
   * @return True, if left associative.
   */
  [[nodiscard]] static std::uint8_t is_left_associative(const Operator operator_) noexcept
  {
    switch (operator_) {
      case Operator::Plus:
      case Operator::Minus:
      case Operator::Times:
      case Operator::Divide:
        return true;
    }

    return false;
  }

  /**
   * Checks if the given char could belong to an identifier (alphanumerical chars, _, ., etc.).
   *
   * @param char_ Char to check.
   * @return True, if the char could belong to an identifier.
   */
  [[nodiscard]] static bool is_identifier_char(const char char_) noexcept
  {
    return std::isalnum(char_) || char_ == '_' || char_ == '.';
  }

  const std::string _input;
};

/**
 * The expression builder translates an expression (string) to an executable metric expression.
 */
class ExpressionBuilder
{
public:
  /**
   * Builds an evaluable expression from the given expression-string.
   *
   * @param expression Expression.
   * @return Evaluable expression.
   */
  [[nodiscard]] static std::unique_ptr<MetricExpressionInterface> build(std::string&& expression);
};
}