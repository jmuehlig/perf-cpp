#pragma once

#include "counter.h"
#include "exception.h"
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace perf::metric::expression {

/**
 * Representation of the supported operators.
 */
enum class Operator_
{
  Plus,
  Minus,
  Times,
  Divide
};

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

/**
 * A token represents a single constant, identifier, operator (like +,-, etc.), or parenthesis.
 */
class Token
{
public:
  enum class Parenthesis : std::uint8_t
  {
    Left,
    Right
  };

  /**
   * Token can be an identifier, a constant number, an operator, or a parenthesis.
   */
  using token_t = std::variant<std::string, double, Operator_, Parenthesis>;

  Token(Token&&) noexcept = default;
  Token(const Token&) = default;

  explicit Token(const Operator_ operator_)
    : _token(operator_)
  {
  }

  explicit Token(const double number)
    : _token(number)
  {
  }

  explicit Token(std::string&& text)
    : _token(std::move(text))
  {
  }

  explicit Token(const Parenthesis parenthesis)
    : _token(parenthesis)
  {
  }

  ~Token() = default;

  Token& operator=(Token&&) noexcept = default;

  /**
   * @return True, if this token is a left parenthesis.
   */
  [[nodiscard]] bool is_left_parenthesis() const noexcept
  {
    return std::holds_alternative<Parenthesis>(_token) && std::get<Parenthesis>(_token) == Parenthesis::Left;
  }

  /**
   * @return True, if this token is a metric operator.
   */
  [[nodiscard]] bool is_operator() const noexcept { return std::holds_alternative<Operator_>(_token); }

  /**
   * @return The operator inside the token.
   */
  [[nodiscard]] Operator_ operator_() const noexcept { return std::get<Operator_>(_token); }

  /**
   * @return Ownership of the underlying token data.
   */
  [[nodiscard]] token_t data() noexcept { return std::move(_token); }

  /**
   * @return A text representation of this token.
   */
  [[nodiscard]] std::string to_string() const;

private:
  token_t _token;

  /**
   * Visits a token and translates it into an std::string.
   */
  class TokenToStringVisitor
  {
  public:
    [[nodiscard]] std::string operator()(const std::string& identifier) const { return identifier; }

    [[nodiscard]] std::string operator()(const double constant) const { return std::to_string(constant); }

    [[nodiscard]] std::string operator()(Operator_ metric_operator) const;

    [[nodiscard]] std::string operator()(Token::Parenthesis parenthesis) const;
  };
};

/**
 * The Tokenizer translates a given input string into a queue of tokens.
 */
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
  /// The expression to tokenize.
  const std::string _input;

  /**
   * Reads a constant number (e.g., 13.37) from the input string, starting at the given position.
   *
   * @param begin Position within the input string.
   * @return A tuple (token containing the constant, new position).
   */
  [[nodiscard]] std::pair<double, std::size_t> read_constant(std::size_t begin) const;

  /**
   * Reads an identifier (e.g., a hardware counter name) from the input string, starting at the given position.
   *
   * @param begin Position within the input string.
   * @return A tuple (token containing the identifier, new position).
   */
  [[nodiscard]] std::pair<std::string, std::size_t> read_identifier(std::size_t begin) const;

  /**
   * Reads an operator (e.g., +) from the given char.
   *
   * @param current_char Current char from input string.
   * @return The metric operator.
   */
  [[nodiscard]] Operator_ read_operator(char current_char) const;

  /**
   * Checks if the given char is an escape character.
   *
   * @param current_char Current character.
   * @return True, if the given char is an escape character.
   */
  [[nodiscard]] static bool is_escape_char(char current_char) noexcept
  {
    return current_char == '\'' || current_char == '`';
  }

  /**
   * Tests if the left operator has greater precedence than the right operator.
   *
   * @param left_operator Operator.
   * @param right_operator Operator.
   * @return True, if the left operator has a greater precedence than the right operator (or if the right operator is
   * left associative if both have the same precedence).
   */
  [[nodiscard]] static bool has_greater_precedence(Operator_ left_operator, Operator_ right_operator) noexcept;

  /**
   * Returns the precedence of the given operator.
   *
   * @param operator_ Operator.
   * @return Precedence of the operator.
   */
  [[nodiscard]] static std::uint8_t precedence(Operator_ operator_) noexcept;

  /**
   * Tests if the operator is left associative.
   *
   * @param operator_ Operator to test.
   * @return True, if left associative.
   */
  [[nodiscard]] static bool is_left_associative(Operator_ operator_) noexcept;

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
};

/**
 * The expression builder translates an expression (string) to an executable metric expression.
 */
class Builder
{
public:
  /**
   * Builds an evaluable expression from the given expression-string.
   *
   * @param input_expression Expression to translate into a metric expression.
   * @return Evaluable expression.
   */
  [[nodiscard]] static std::unique_ptr<ExpressionInterface> build(std::string&& input_expression);

private:
  /**
   * Visits the token and translates the token into a metric expression that can be pushed to the stack.
   */
  class TokenToMetricExpressionVisitor
  {
  public:
    TokenToMetricExpressionVisitor(const std::string& input,
                                   std::stack<std::unique_ptr<ExpressionInterface>>& expression_stack) noexcept
      : _input(input)
      , _expression_stack(expression_stack)
    {
    }
    ~TokenToMetricExpressionVisitor() = default;

    [[nodiscard]] std::unique_ptr<ExpressionInterface> operator()(std::string&& identifier)
    {
      return std::make_unique<IdentifierExpression>(std::move(identifier));
    }

    [[nodiscard]] std::unique_ptr<ExpressionInterface> operator()(const double constant)
    {
      return std::make_unique<ConstantExpression>(constant);
    }

    [[nodiscard]] std::unique_ptr<ExpressionInterface> operator()(
      [[maybe_unused]] const Token::Parenthesis /* parenthesis */) const
    {
      /// Ignore parenthesis.
      return nullptr;
    }

    [[nodiscard]] std::unique_ptr<ExpressionInterface> operator()(Operator_ metric_operator);

  private:
    /// Input of the expression, used to throw an exception.
    const std::string& _input;

    /// Expression stack to push expressions to (and pop operators).
    std::stack<std::unique_ptr<ExpressionInterface>>& _expression_stack;
  };
};
}