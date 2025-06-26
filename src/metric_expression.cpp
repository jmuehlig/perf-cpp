#include <perfcpp/metric_expression.h>
#include <stack>

std::string
perf::Token::to_string() const
{
  switch (this->_type) {
    case Type::ConstantNumber:
      return std::string{ "constant(" }.append(std::to_string(_number.value())).append(")");
    case Type::Identifier:
      return std::string{ "identifier(" }.append(_text.value()).append(")");
    case Type::Operator:
      switch (this->_operator.value()) {
        case MetricOperator::Plus:
          return "+";
        case MetricOperator::Minus:
          return "-";
        case MetricOperator::Times:
          return "*";
        case MetricOperator::Divide:
          return "/";
        default:
          return "<unknown operator>";
      }
    case Type::LeftParenthesis:
      return "(";
    case Type::RightParenthesis:
      return ")";
    default:
      return "<unknown token>";
  }
}

std::queue<perf::Token>
perf::Tokenizer::tokenize() const
{
  /// Tokenize the input string using the Shunting yard algorithm.
  /// For more information see: https://en.wikipedia.org/wiki/Shunting_yard_algorithm

  auto output_queue = std::queue<Token>{};
  auto operator_stack = std::stack<Token>{};

  auto position = std::size_t{ 0U };

  while (position < this->_input.length()) {
    /// Skip all whitespaces.
    while (position < this->_input.length() && std::isspace(this->_input[position])) {
      ++position;
    }

    const auto current_char = this->_input[position];

    /// Check if the next character is a constant number (obviously a digit indicates a number – and so does a ".").
    /// If so, return a number token.
    if (std::isdigit(current_char) || current_char == '.') {
      auto [constant_token, new_position] = Tokenizer::read_constant(position);
      output_queue.push(std::move(constant_token));
      position = new_position;
      continue;
    }

    /// Check if the next character is an alphabetical char, which indicates an identifier.
    /// Additionally, identifiers can start with single quotes to escape, for example, - operators as part of the
    /// identifier (e.g., the hardware counter "L1-cache-miss"). If so, return an identifier token.
    if (std::isalpha(current_char) || current_char == '\'') {
      auto [identifier_token, new_position] = Tokenizer::read_identifier(position);
      output_queue.push(std::move(identifier_token));
      position = new_position;
      continue;
    }

    if (!std::isspace(current_char)) {
      /// Handle parentheses and operators.
      if (current_char == '(') {
        operator_stack.emplace(Token::Type::LeftParenthesis);
      } else if (current_char == ')') {
        /// Move all operators (except the left parenthesis) from the OP stack to the token queue.
        while (!operator_stack.empty() && operator_stack.top() != Token::Type::LeftParenthesis) {
          output_queue.push(operator_stack.top());
          operator_stack.pop();
        }

        /// Pop the remaining left parenthesis.
        operator_stack.pop();
      } else {
        const auto operator_ = Tokenizer::read_operator(current_char);
        while (!operator_stack.empty() && operator_stack.top() != Token::Type::LeftParenthesis &&
               Tokenizer::has_greater_precedence(operator_stack.top(), operator_)) {
          output_queue.push(operator_stack.top());
          operator_stack.pop();
        }
        operator_stack.push(operator_);
      }

      ++position;
    }
  }

  /// Move the remaining tokens from the operator stack into the token queue.
  while (!operator_stack.empty()) {
    if (operator_stack.top() == Token::Type::LeftParenthesis) {
      throw CannotParseExpressionError{ this->_input };
    }
    output_queue.push(operator_stack.top());
    operator_stack.pop();
  }

  return output_queue;
}

std::pair<perf::Token, std::size_t>
perf::Tokenizer::read_constant(const std::size_t begin) const
{
  /// We know that the current character (this->_position) is a digit; otherwise, this function wouldn't have been
  /// called.
  auto count = 1ULL;

  /// Read all characters that are digits or a dot.
  /// Additionally, check if the number has only one dot.
  auto has_dot = false;
  while ((begin + count) < this->_input.length() &&
         (std::isdigit(this->_input[begin + count]) || this->_input[begin + count] == '.')) {
    if (this->_input[begin + count] == '.' && std::exchange(has_dot, true)) {
      throw CannotParseExpressionError{ this->_input };
    }
    ++count;
  }

  return std::make_pair(Token{ std::stod(_input.substr(begin, count)) }, begin + count);
}

std::pair<perf::Token, std::size_t>
perf::Tokenizer::read_identifier(std::size_t begin) const
{
  /// We know that the current character (this->_position) is alphabetical; otherwise, this function wouldn't have been
  /// called.
  auto count = 1ULL;

  /// If the identifier starts with a single quote, we scan until we find the "ending" single quote.
  const auto is_start_with_quote = this->_input[begin] == '\'';
  if (is_start_with_quote) {
    while ((begin + count) < this->_input.size() && this->_input[begin + count] != '\'') {
      ++count;
    }

    /// If we reached the end of the string, the quote was never closed.
    if ((begin + count) == this->_input.size()) {
      throw CannotParseExpressionError{ this->_input, "Open quote was never closed" };
    }
  }
  /// Otherwise, we read all characters that are alphabetical or numerical(e.g., L2Cache).
  else {
    while ((begin + count) < this->_input.size() && Tokenizer::is_identifier_char(this->_input[begin + count])) {
      ++count;
    }
  }

  /// Increase the position by the number of scanned chars; skip the closing single quite if given.
  const auto new_position = begin + count + static_cast<std::uint64_t>(is_start_with_quote);

  /// Return the identifier; remove single quotes if given.
  auto token = Token{ this->_input.substr(begin + static_cast<std::uint64_t>(is_start_with_quote),
                                          count - static_cast<std::uint64_t>(is_start_with_quote)) };
  return std::make_pair(std::move(token), new_position);
}

perf::Token
perf::Tokenizer::read_operator(const char current_char) const
{
  /// Translate the given char into an operator, if it is one.
  switch (current_char) {
    case '+':
      return Token{ MetricOperator::Plus };
    case '-':
      return Token{ MetricOperator::Minus };
    case '*':
      return Token{ MetricOperator::Times };
    case '/':
      return Token{ MetricOperator::Divide };

      /// We could not tokenize a number, a sequence of chars. or an operator. This is an error.
    default:
      throw CannotParseExpressionError{ this->_input, "Unknown operator token" };
  }
}

std::unique_ptr<perf::MetricExpressionInterface>
perf::ExpressionBuilder::build(std::string&& expression)
{
  auto tokenizer = Tokenizer{ std::move(expression) };
  auto token_queue = tokenizer.tokenize();

  /// The expression stack will be built and consumed while scanning the tokens.
  auto expression_stack = std::stack<std::unique_ptr<MetricExpressionInterface>>{};

  /// Scan the tokenized queue: Push identifier and constants to the expression stack and consume them by binary
  /// expressions.
  while (!token_queue.empty()) {
    /// Fetch the next token from the queue.
    auto token = std::move(token_queue.front());
    token_queue.pop();

    if (token == Token::Type::Identifier) {
      expression_stack.push(std::make_unique<IdentifierExpression>(std::move(token.text().value())));
    } else if (token == Token::Type::ConstantNumber) {
      expression_stack.push(std::make_unique<ConstantExpression>(token.number().value()));
    } else if (token == Token::Type::Operator) {
      /// Verify that there are at least two expressions that can be consumed by the binary expression.
      if (expression_stack.size() < 2U) {
        throw CannotEvaluateExpressionError{ tokenizer.input() };
      }

      /// Read the two top expressions.
      std::unique_ptr<MetricExpressionInterface> right_expression = std::move(expression_stack.top());
      expression_stack.pop();
      std::unique_ptr<MetricExpressionInterface> left_expression = std::move(expression_stack.top());
      expression_stack.pop();

      /// Create the binary expression.
      switch (token.op().value()) {
        case MetricOperator::Plus:
          expression_stack.push(std::make_unique<BinaryExpression<MetricOperator::Plus>>(std::move(left_expression),
                                                                                         std::move(right_expression)));
          break;
        case MetricOperator::Minus:
          expression_stack.push(std::make_unique<BinaryExpression<MetricOperator::Minus>>(std::move(left_expression),
                                                                                          std::move(right_expression)));
          break;
        case MetricOperator::Times:
          expression_stack.push(std::make_unique<BinaryExpression<MetricOperator::Times>>(std::move(left_expression),
                                                                                          std::move(right_expression)));
          break;
        case MetricOperator::Divide:
          expression_stack.push(std::make_unique<BinaryExpression<MetricOperator::Divide>>(
            std::move(left_expression), std::move(right_expression)));
          break;
      }
    }
  }

  /// There should be one root expression left.
  if (expression_stack.size() != 1U) {
    throw CannotEvaluateExpressionError{ tokenizer.input() };
  }

  return std::move(expression_stack.top());
}
