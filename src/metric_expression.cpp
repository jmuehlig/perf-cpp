#include <perfcpp/metric_expression.h>

std::string
perf::metric::expression::Token::TokenToStringVisitor::operator()(const perf::metric::expression::Operator_ metric_operator) const
{
  switch (metric_operator) {
    case Operator_::Plus:
      return "+";
    case Operator_::Minus:
      return "-";
    case Operator_::Times:
      return "*";
    case Operator_::Divide:
      return "/";
    default:
      return "<unknown operator>";
  }
}

std::string
perf::metric::expression::Token::TokenToStringVisitor::operator()(const perf::metric::expression::Token::Parenthesis parenthesis) const
{
  switch (parenthesis) {
    case Token::Parenthesis::Left:
      return "(";
    case Token::Parenthesis::Right:
      return ")";
    default:
      return "<unknown parenthesis>";
  }
}

std::string
perf::metric::expression::Token::to_string() const
{
  return std::visit(TokenToStringVisitor{}, this->_token);
}

std::queue<perf::metric::expression::Token>
perf::metric::expression::Tokenizer::tokenize() const
{
  /// Tokenize the input string using the Shunting yard algorithm.
  /// For more information see: https://en.wikipedia.org/wiki/Shunting_yard_algorithm

  auto output_queue = std::queue<Token>{};
  auto operator_stack = std::stack<Token>{};

  auto position = 0ULL;

  while (position < this->_input.length()) {
    /// Skip all whitespaces.
    while (position < this->_input.length() && std::isspace(this->_input[position])) {
      ++position;
    }

    /// Get the next char (that is not a whitespace).
    const auto current_char = this->_input[position];

    /// Check if the next character is a constant number (obviously a digit indicates a number – and so does a ".").
    /// If so, return a number token.
    if (std::isdigit(current_char) || current_char == '.') {
      auto [constant_token, new_position] = Tokenizer::read_constant(position);

      /// The constant goes to the queue.
      output_queue.emplace(constant_token);

      /// The constant tokenizer calculates the next position.
      position = new_position;
    }

    /// Check if the next character is an alphabetical char, which indicates an identifier.
    /// Additionally, identifiers can start with single quotes to escape, for example, - operators as part of the
    /// identifier (e.g., the hardware counter "L1-cache-miss"). If so, return an identifier token.
    else if (std::isalpha(current_char) || Tokenizer::is_escape_char(current_char)) {
      auto [identifier_token, new_position] = Tokenizer::read_identifier(position);

      /// The identifier goes to the queue.
      output_queue.emplace(std::move(identifier_token));

      /// The identifier tokenizer calculates the next position.
      position = new_position;
    }

    /// Handle opened parentheses.
    else if (current_char == '(') {
      /// An opened parenthesis goes to the queue.
      operator_stack.emplace(Token::Parenthesis::Left);

      /// Move forward to the next character.
      ++position;
    }

    /// Handle closing parenthesis.
    else if (current_char == ')') {
      /// Move all operators (except the left parenthesis) from the OP stack to the token queue.
      while (!operator_stack.empty() && !operator_stack.top().is_left_parenthesis()) {
        output_queue.push(operator_stack.top());
        operator_stack.pop();
      }

      /// Pop the remaining left parenthesis.
      operator_stack.pop();

      /// Move forward to the next character.
      ++position;
    }

    /// Every other character is tokenized as an operator.
    else {
      const auto operator_ = Tokenizer::read_operator(current_char);

      while (!operator_stack.empty() && operator_stack.top().is_operator() &&
             !operator_stack.top().is_left_parenthesis() &&
             Tokenizer::has_greater_precedence(operator_stack.top().operator_(), operator_)) {
        output_queue.push(operator_stack.top());
        operator_stack.pop();
      }

      /// The operator goes to the queue.
      operator_stack.emplace(operator_);

      /// Move forward to the next character.
      ++position;
    }
  }

  /// Move the remaining tokens from the operator stack into the token queue.
  while (!operator_stack.empty()) {
    if (operator_stack.top().is_left_parenthesis()) {
      throw CannotParseExpressionError{ this->_input };
    }
    output_queue.push(operator_stack.top());
    operator_stack.pop();
  }

  return output_queue;
}

std::pair<double, std::size_t>
perf::metric::expression::Tokenizer::read_constant(const std::size_t begin) const
{
  /// We know that the current character (this->_position) is a digit; otherwise, this function wouldn't have been
  /// called.
  auto count = 1ULL;

  /// Read all characters that are digits or a dot.
  /// Additionally, check if the number has only one dot.
  auto has_dot = false;
  auto has_scientific_e = false;
  while ((begin + count) < this->_input.length() &&
         (std::isdigit(this->_input[begin + count]) || this->_input[begin + count] == '-' ||
          this->_input[begin + count] == '.' || this->_input[begin + count] == 'e' ||
          this->_input[begin + count] == 'E')) {

    /// Verify that only one dot is in the number.
    if (this->_input[begin + count] == '.' && std::exchange(has_dot, true)) {
      throw CannotParseExpressionError{ this->_input };
    }

    /// Verify that only one scientific e is in the number.
    if ((this->_input[begin + count] == 'e' || this->_input[begin + count] == 'E') &&
        std::exchange(has_scientific_e, true)) {
      throw CannotParseExpressionError{ this->_input };
    }

    ++count;
  }

  /// Extract the number.
  const auto number = _input.substr(begin, count);

  /// Ensure the number is not empty.
  if (number.empty()) {
    throw CannotParseExpressionError{ this->_input };
  }

  /// Ensure the number does not end with minus or scientific e.
  if (number.back() == '-' || number.back() == 'e' || number.back() == 'E') {
    throw CannotParseExpressionError{ this->_input };
  }

  /// Parse number into decimal.
  return std::make_pair(std::stod(number), begin + count);
}

std::pair<std::string, std::size_t>
perf::metric::expression::Tokenizer::read_identifier(const std::size_t begin) const
{
  /// We know that the current character (this->_position) is alphabetical; otherwise, this function wouldn't have been
  /// called.
  auto count = 1ULL;

  /// If the identifier starts with a single quote, we scan until we find the "ending" single quote.
  const auto is_start_with_escape_char = Tokenizer::is_escape_char(this->_input[begin]);
  if (is_start_with_escape_char) {
    while ((begin + count) < this->_input.size() && !Tokenizer::is_escape_char(this->_input[begin + count])) {
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
  const auto new_position = begin + count + static_cast<std::uint64_t>(is_start_with_escape_char);

  /// Return the identifier; remove single quotes if given.
  auto identifier = this->_input.substr(begin + static_cast<std::uint64_t>(is_start_with_escape_char),
                                        count - static_cast<std::uint64_t>(is_start_with_escape_char));
  return std::make_pair(std::move(identifier), new_position);
}

perf::metric::expression::Operator_
perf::metric::expression::Tokenizer::read_operator(const char current_char) const
{
  /// Translate the given char into an operator, if it is one.
  switch (current_char) {
    case '+':
      return Operator_::Plus;
    case '-':
      return Operator_::Minus;
    case '*':
      return Operator_::Times;
    case '/':
      return Operator_::Divide;

      /// We could not tokenize a number, a sequence of chars. or an operator. This is an error.
    default:
      throw CannotParseExpressionError{ this->_input, "Unknown operator token" };
  }
}

bool
perf::metric::expression::Tokenizer::has_greater_precedence(const perf::metric::expression::Operator_ left_operator,
                                                const perf::metric::expression::Operator_ right_operator) noexcept
{
  const auto left_precedence = Tokenizer::precedence(left_operator);
  const auto right_precedence = Tokenizer::precedence(right_operator);

  return (Tokenizer::is_left_associative(right_operator) && right_precedence <= left_precedence) ||
         (right_precedence < left_precedence);
}

std::uint8_t
perf::metric::expression::Tokenizer::precedence(const perf::metric::expression::Operator_ operator_) noexcept
{
  switch (operator_) {
    case Operator_::Plus:
    case Operator_::Minus:
      return 4U;
    case Operator_::Times:
    case Operator_::Divide:
      return 8U;
  }

  return 0U;
}

bool
perf::metric::expression::Tokenizer::is_left_associative(const perf::metric::expression::Operator_ operator_) noexcept
{
  switch (operator_) {
    case Operator_::Plus:
    case Operator_::Minus:
    case Operator_::Times:
    case Operator_::Divide:
      return true;
    default:
      return false;
  }
}

std::unique_ptr<perf::metric::expression::ExpressionInterface>
perf::metric::expression::Builder::TokenToMetricExpressionVisitor::operator()(
  const perf::metric::expression::Operator_ metric_operator)
{
  /// Verify that there are at least two expressions that can be consumed by the binary expression.
  if (this->_expression_stack.size() < 2U) {
    throw CannotEvaluateExpressionError{ this->_input };
  }

  /// Read the two top expressions from the stack.
  auto right_expression = std::move(this->_expression_stack.top());
  this->_expression_stack.pop();
  auto left_expression = std::move(this->_expression_stack.top());
  this->_expression_stack.pop();

  /// Translate the operator into an expression.
  switch (metric_operator) {
    case Operator_::Plus:
      return std::make_unique<BinaryExpression<Operator_::Plus>>(std::move(left_expression),
                                                                 std::move(right_expression));
    case Operator_::Minus:
      return std::make_unique<BinaryExpression<Operator_::Minus>>(std::move(left_expression),
                                                                  std::move(right_expression));
    case Operator_::Times:
      return std::make_unique<BinaryExpression<Operator_::Times>>(std::move(left_expression),
                                                                  std::move(right_expression));
    case Operator_::Divide:
      return std::make_unique<BinaryExpression<Operator_::Divide>>(std::move(left_expression),
                                                                   std::move(right_expression));
    default:
      return nullptr;
  }
}

std::unique_ptr<perf::metric::expression::ExpressionInterface>
perf::metric::expression::Builder::build(std::string&& input_expression)
{
  auto tokenizer = Tokenizer{ std::move(input_expression) };
  auto token_queue = tokenizer.tokenize();

  /// The expression stack will be built and consumed while scanning the tokens.
  auto expression_stack = std::stack<std::unique_ptr<ExpressionInterface>>{};

  /// Scan the tokenized queue: Push identifier and constants to the expression stack and consume them by binary
  /// expressions.
  while (!token_queue.empty()) {
    /// Fetch the next token from the queue.
    auto token = std::move(token_queue.front());
    token_queue.pop();

    /// The TokenVisitor will visit the token data and builds an expression. For operators, the visitor pops expressions
    /// from the stack.
    if (auto metric_expression = std::visit(Builder::TokenToMetricExpressionVisitor{ tokenizer.input(), expression_stack }, token.data());
        metric_expression != nullptr) {
      expression_stack.push(std::move(metric_expression));
    }
  }

  /// There should be one root expression left.
  if (expression_stack.size() != 1U) {
    throw CannotEvaluateExpressionError{ tokenizer.input() };
  }

  return std::move(expression_stack.top());
}
