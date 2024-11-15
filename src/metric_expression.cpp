#include <perfcpp/metric_expression.h>

perf::Token
perf::Tokenizer::next()
{
  /// Skip all whitespaces.
  while (this->_position < this->_input.length() && std::isspace(this->_input[this->_position])) {
    ++this->_position;
  }

  /// Check if we reached the end.
  if (this->_position == this->_input.size() || this->_input[this->_position] == '\0') {
    return Token{};
  }

  const auto current_char = this->_input[this->_position];

  /// Check if the next character is a constant number (obviously a digit indicates a number – and so does a ".").
  /// If so, return a number token.
  if (std::isdigit(current_char) || current_char == '.') {
    return this->read_constant_number();
  }

  /// Check if the next character is an alphabetical char, which indicates an identifier.
  /// Additionally, identifiers can start with single quotes to escape, for example, - operators as part of the
  /// identifier (e.g., the hardware counter "L1-cache-miss"). If so, return an identifier token.
  if (std::isalpha(current_char) || current_char == '\'') {
    return this->read_identifier();
  }

  /// Reading a number and an identifier increase _position until the end of the number or identifier.
  /// For all other single-char tokens, we make progress.
  ++_position;

  switch (current_char) {
    case '+':
      return Token{ Operator::Plus };
    case '-':
      return Token{ Operator::Minus };
    case '*':
      return Token{ Operator::Times };
    case '/':
      return Token{ Operator::Divide };
    case '(':
      return Token{ Token::Type::LeftParenthesis };
    case ')':
      return Token{ Token::Type::RightParenthesis };

      /// We could not tokenize a number, a sequence of chars. or an operator. This is an error.
    default:
      throw CannotParseExpressionError{ _input };
  }
}

perf::Token
perf::Tokenizer::read_constant_number()
{
  const auto begin = this->_position;

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

  this->_position += count;
  return Token{ std::stod(_input.substr(begin, count)) };
}

perf::Token
perf::Tokenizer::read_identifier()
{
  const auto begin = this->_position;

  /// We know that the current character (this->_position) is alphabetical; otherwise, this function wouldn't have been
  /// called.
  auto count = 1ULL;

  /// If the identifier starts with a single quote, we scan until we find the "ending" single quote.
  const auto starts_with_single_quote = this->_input[begin] == '\'';
  if (starts_with_single_quote) {
    while ((begin + count) < this->_input.size() && this->_input[begin + count] != '\'') {
      ++count;
    }
  }
  /// Otherwise, we read all characters that are alphabetical or numerical(e.g., L2Cache).
  else {
    while ((begin + count) < this->_input.size() && Tokenizer::is_identifier_char(this->_input[begin + count])) {
      ++count;
    }
  }

  /// Increase the position by the number of scanned chars; skip the closing single quite if given.
  this->_position += count + static_cast<std::uint64_t>(starts_with_single_quote);

  /// Return the identifier; remove single quotes if given.
  return Token{ this->_input.substr(begin + static_cast<std::uint64_t>(starts_with_single_quote),
                                    count - static_cast<std::uint64_t>(starts_with_single_quote)) };
}

std::unique_ptr<perf::MetricExpression>
perf::Parser::parse_dash_operation()
{
  /// Parse the next expression.
  auto left_expression = this->parse_dot_expression();

  /// If the token after the left expression is an operator, we have a binary expression.
  if (const auto token = this->_tokenizer.next(); token.is_dash_operation()) {

    /// Parse the right side and build the BinaryExpression.
    auto right_expression = this->parse_dot_expression();

    if (token.operator_().value() == Operator::Plus) {
      return std::make_unique<BinaryExpression<Operator::Plus>>(std::move(left_expression),
                                                                std::move(right_expression));
    } else if (token.operator_().value() == Operator::Minus) {
      return std::make_unique<BinaryExpression<Operator::Minus>>(std::move(left_expression),
                                                                 std::move(right_expression));
    }
  }

  return left_expression;
}

std::unique_ptr<perf::MetricExpression>
perf::Parser::parse_dot_expression()
{
  /// Parse the next expression.
  auto left_expression = this->parse_factor();

  /// If the token after the left expression is an operator, we have a binary expression.
  if (const auto token = this->_tokenizer.next(); token.is_dot_operation()) {

    /// Parse the right side and build the BinaryExpression.
    auto right_expression = this->parse_dot_expression();

    if (token.operator_().value() == Operator::Times) {
      return std::make_unique<BinaryExpression<Operator::Times>>(std::move(left_expression),
                                                                 std::move(right_expression));
    } else if (token.operator_().value() == Operator::Divide) {
      return std::make_unique<BinaryExpression<Operator::Divide>>(std::move(left_expression),
                                                                  std::move(right_expression));
    }
  }

  return left_expression;
}

std::unique_ptr<perf::MetricExpression>
perf::Parser::parse_factor()
{
  /// Read the next token.
  Token token = this->_tokenizer.next();

  if (token == Token::Type::ConstantNumber) {
    return std::make_unique<ConstantExpression>(token.number().value());
  }

  if (token == Token::Type::Identifier) {
    return std::make_unique<IdentifierExpression>(std::move(token.text().value()));
  }

  if (token == Token::Type::LeftParenthesis) {
    /// Read the expression that is inside the parentheses.
    auto expression = this->parse();

    /// Verify that the token after the expression is a closing the parenthesis.
    if (this->_tokenizer.next() == Token::Type::RightParenthesis) {
      return expression;
    }

    throw CannotParseExpressionError{ this->_tokenizer.input(), "Opened parenthesis is not closed." };
  }

  throw CannotParseExpressionError{ this->_tokenizer.input(), "Unexpected token." };
}