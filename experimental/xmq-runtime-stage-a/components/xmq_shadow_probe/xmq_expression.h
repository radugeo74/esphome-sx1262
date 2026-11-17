#pragma once
// Experimental XMQ numeric expression evaluator, independent of ESPHome.
// Deliberately rejects unsupported syntax (especially date/calendar arithmetic).
#include <cmath>
#include <cctype>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

namespace xmq_runtime {

enum class Dimension { Counter, VolumeM3, HCA, TemperatureC };
struct Number {
  double value = 0;
  Dimension dimension = Dimension::Counter;
};
struct ExpressionError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class NumericExpression {
 public:
  explicit NumericExpression(const std::map<std::string, Number> &variables)
      : variables_(variables) {}

  Number evaluate(const std::string &expression) {
    if (expression.size() > 4096) throw ExpressionError("expression too long");
    input_ = expression;
    pos_ = 0;
    budget_ = 0;
    auto result = logical_or(0);
    skip_ws();
    if (pos_ != input_.size()) throw ExpressionError("unsupported trailing syntax");
    if (!std::isfinite(result.value)) throw ExpressionError("nonfinite result");
    return result;
  }

 private:
  const std::map<std::string, Number> &variables_;
  std::string input_;
  size_t pos_ = 0;
  size_t budget_ = 0;

  void skip_ws() {
    while (pos_ < input_.size() &&
           std::isspace(static_cast<unsigned char>(input_[pos_]))) ++pos_;
  }
  bool consume(const std::string &op) {
    skip_ws();
    if (input_.compare(pos_, op.size(), op) == 0) {
      pos_ += op.size();
      return true;
    }
    return false;
  }
  void guard(unsigned depth) {
    if (depth > 192 || ++budget_ > 2048) throw ExpressionError("expression complexity limit");
  }
  static Number counter(double v) {
    if (!std::isfinite(v)) throw ExpressionError("nonfinite arithmetic");
    return {v, Dimension::Counter};
  }
  static Number arithmetic(Number a, Number b, char op) {
    if (op == '+' || op == '-') {
      if (a.dimension != b.dimension) throw ExpressionError("incompatible dimensions");
      return {op == '+' ? a.value + b.value : a.value - b.value, a.dimension};
    }
    if (op == '*') {
      if (a.dimension != Dimension::Counter &&
          b.dimension != Dimension::Counter)
        throw ExpressionError("multiplication of two physical units unsupported");
      return {a.value * b.value,
              a.dimension != Dimension::Counter ? a.dimension : b.dimension};
    }
    if (op == '/') {
      if (b.value == 0) throw ExpressionError("division by zero");
      if (b.dimension != Dimension::Counter && a.dimension != b.dimension)
        throw ExpressionError("inverse physical unit not supported");
      return {a.value / b.value, a.dimension == b.dimension ?
                                   Dimension::Counter : a.dimension};
    }
    throw ExpressionError("unknown arithmetic");
  }
  static int64_t integer(Number n) {
    if (n.dimension != Dimension::Counter || !std::isfinite(n.value) ||
        n.value < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
        n.value >= 9223372036854775808.0 || std::trunc(n.value) != n.value)
      throw ExpressionError("integer operation requires integral counter");
    return static_cast<int64_t>(n.value);
  }
  static Number compare(Number a, Number b, const std::string &op) {
    if (a.dimension != b.dimension) throw ExpressionError("comparison dimension mismatch");
    if (op == "==") return counter(a.value == b.value);
    if (op == "!=") return counter(a.value != b.value);
    if (op == "<=") return counter(a.value <= b.value);
    if (op == ">=") return counter(a.value >= b.value);
    if (op == "<") return counter(a.value < b.value);
    return counter(a.value > b.value);
  }
  static bool truth(Number a) {
    if (a.dimension != Dimension::Counter) throw ExpressionError("logical unit mismatch");
    return a.value != 0;
  }
  Number logical_or(unsigned d) {
    guard(d);
    auto a = logical_and(d+1);
    while (consume("||")) {
      auto b = logical_and(d+1);
      a = counter(truth(a) || truth(b));
    }
    return a;
  }
  Number logical_and(unsigned d) {
    guard(d);
    auto a = comparison(d+1);
    while (consume("&&")) {
      auto b = comparison(d+1);
      a = counter(truth(a) && truth(b));
    }
    return a;
  }
  Number comparison(unsigned d) {
    guard(d);
    auto a = shifts(d+1);
    while (true) {
      std::string op;
      for (const auto &choice : {"<=", ">=", "==", "!=", "<", ">"}) {
        if (consume(choice)) { op = choice; break; }
      }
      if (op.empty()) break;
      a = compare(a, shifts(d+1), op);
    }
    return a;
  }
  Number shifts(unsigned d) {
    guard(d);
    auto a = additive(d+1);
    while (true) {
      const bool right = consume(">>");
      if (!right && !consume("<<")) break;
      auto b = additive(d+1);
      const auto n = integer(a), shift = integer(b);
      if (shift < 0 || shift > 62 || n < 0)
        throw ExpressionError("unsupported shift");
      const auto un = static_cast<uint64_t>(n);
      if (!right && un > (static_cast<uint64_t>(INT64_MAX) >> shift))
        throw ExpressionError("shift overflow");
      a = counter(static_cast<double>(right ? (un >> shift) : (un << shift)));
    }
    return a;
  }
  Number additive(unsigned d) {
    guard(d);
    auto a = multiply(d+1);
    while (true) {
      if (consume("+")) a = arithmetic(a,multiply(d+1),'+');
      else if (consume("-")) a = arithmetic(a,multiply(d+1),'-');
      else break;
    }
    return a;
  }
  Number multiply(unsigned d) {
    guard(d);
    auto a = unary(d+1);
    while (true) {
      if (consume("*")) a = arithmetic(a,unary(d+1),'*');
      else if (consume("/")) a = arithmetic(a,unary(d+1),'/');
      else if (consume("%")) {
        auto b = unary(d+1);
        const auto divisor = integer(b);
        if (divisor == 0) throw ExpressionError("modulo by zero");
        a = counter(static_cast<double>(integer(a) % divisor));
      } else break;
    }
    return a;
  }
  Number unary(unsigned d) {
    guard(d);
    if (consume("+")) return unary(d+1);
    if (consume("-")) {
      auto a = unary(d+1);
      a.value = -a.value;
      return a;
    }
    if (consume("!")) return counter(!truth(unary(d+1)));
    if (consume("(")) {
      auto a = logical_or(d+1);
      if (!consume(")")) throw ExpressionError("missing closing parenthesis");
      return a;
    }
    return atom();
  }
  Number atom() {
    skip_ws();
    if (pos_ >= input_.size()) throw ExpressionError("missing operand");
    const size_t start = pos_;
    if (std::isdigit(static_cast<unsigned char>(input_[pos_])) ||
        input_[pos_] == '.') {
      bool digit = false;
      while (pos_ < input_.size() &&
             (std::isdigit(static_cast<unsigned char>(input_[pos_])) ||
              input_[pos_] == '.')) {
        digit |= std::isdigit(static_cast<unsigned char>(input_[pos_]));
        ++pos_;
      }
      if (!digit) throw ExpressionError("invalid number");
      const auto part = input_.substr(start,pos_-start);
      size_t consumed = 0;
      double value = 0;
      try { value = std::stod(part,&consumed); }
      catch (const std::exception &) { throw ExpressionError("invalid numeric literal"); }
      if (consumed != part.size() || !std::isfinite(value))
        throw ExpressionError("invalid numeric literal");
      const size_t suffix = pos_;
      while (pos_ < input_.size() &&
             (std::isalnum(static_cast<unsigned char>(input_[pos_])) ||
              input_[pos_] == '_')) ++pos_;
      const auto unit = input_.substr(suffix,pos_-suffix);
      if (unit.empty() || unit == "counter") return counter(value);
      if (unit == "m3") return {value,Dimension::VolumeM3};
      throw ExpressionError("unsupported numeric unit: " + unit);
    }
    if (std::isalpha(static_cast<unsigned char>(input_[pos_])) ||
        input_[pos_] == '_') {
      while (pos_ < input_.size() &&
             (std::isalnum(static_cast<unsigned char>(input_[pos_])) ||
              input_[pos_] == '_')) ++pos_;
      const auto name = input_.substr(start,pos_-start);
      auto it = variables_.find(name);
      if (it == variables_.end()) throw ExpressionError("unresolved variable: " + name);
      return it->second;
    }
    throw ExpressionError("unsupported token");
  }
};

}  // namespace xmq_runtime
