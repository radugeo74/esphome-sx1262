#ifdef error
#pragma push_macro("error")
#undef error
#define XMQ_READER_RESTORE_ERROR_MACRO
#endif
#pragma once
// Stage A: standalone, bounded structural XMQ reader (NOT an XMQ evaluator).
// C++17; no ESPHome or wM-Bus radio dependencies.
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xmq_runtime {

struct Node {
  std::string key;
  std::string value;
  std::vector<Node> children;
  bool block = false;
  const Node *first(const std::string &name) const {
    for (const auto &child : children)
      if (child.key == name) return &child;
    return nullptr;
  }
};

struct ParseError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class Parser {
 public:
  // Reject malicious or accidental large rulesets before parsing.
  explicit Parser(size_t max_bytes = 256 * 1024, size_t max_nodes = 4096,
                  size_t max_depth = 32)
      : max_bytes_(max_bytes), max_nodes_(max_nodes), max_depth_(max_depth) {}

  std::vector<Node> parse(const std::string &src) {
    if (src.size() > max_bytes_) throw ParseError("XMQ source size limit");
    tokenize(src);
    pos_ = 0;
    nodes_ = 0;
    return parse_children(0, false);
  }

 private:
  struct Token {
    std::string value;
    bool quoted = false;
    size_t line = 1;
  };
  std::vector<Token> tokens_;
  size_t pos_ = 0;
  size_t nodes_ = 0;
  size_t max_bytes_, max_nodes_, max_depth_;

  [[noreturn]] static void error(const std::string &what, size_t line) {
    throw ParseError("XMQ line " + std::to_string(line) + ": " + what);
  }

  void tokenize(const std::string &s) {
    tokens_.clear();
    size_t line = 1;
    for (size_t i = 0; i < s.size();) {
      const char c = s[i];
      if (c == '\n') { tokens_.push_back({"\n", false, line++}); ++i; continue; }
      if (std::isspace(static_cast<unsigned char>(c))) { ++i; continue; }
      if (c == '/' && i + 1 < s.size() && s[i+1] == '/') {
        i += 2;
        while (i < s.size() && s[i] != '\n') ++i;
        continue;
      }
      if (c == '/' && i + 1 < s.size() && s[i+1] == '*') {
        const size_t start = line;
        i += 2;
        bool closed = false;
        while (i + 1 < s.size()) {
          if (s[i] == '\n') ++line;
          if (s[i] == '*' && s[i+1] == '/') { i += 2; closed = true; break; }
          ++i;
        }
        if (!closed) error("unterminated comment", start);
        continue;
      }
      if (c == '\'' || c == '"') {
        const char quote = c;
        const size_t start = line;
        ++i;
        std::string value;
        bool closed = false;
        while (i < s.size()) {
          const char d = s[i++];
          if (d == '\n') ++line;
          if (d == '\\' && i < s.size()) {
            // Keep escape sequences as source text; evaluator owns semantics.
            value += d;
            value += s[i++];
            continue;
          }
          if (d == quote) { closed = true; break; }
          value += d;
        }
        if (!closed) error("unterminated string", start);
        tokens_.push_back({std::move(value), true, start});
        continue;
      }
      if (c == '{' || c == '}' || c == '=' || c == '(' || c == ')') {
        tokens_.push_back({std::string(1,c), false, line});
        ++i;
        continue;
      }
      const size_t start = i;
      while (i < s.size() &&
             !std::isspace(static_cast<unsigned char>(s[i])) &&
             s[i] != '{' && s[i] != '}' && s[i] != '=' &&
             s[i] != '(' && s[i] != ')' &&
             !(s[i] == '/' && i + 1 < s.size() &&
               (s[i+1] == '/' || s[i+1] == '*'))) ++i;
      if (start == i) error("unexpected input", line);
      tokens_.push_back({s.substr(start, i-start), false, line});
    }
  }

  static std::string join(const std::vector<Token> &a, size_t b, size_t e) {
    std::string out;
    for (size_t i = b; i < e; ++i) {
      if (!out.empty() && a[i].value != ")" && a[i].value != "(" &&
          a[i-1].value != "(") out += ' ';
      out += a[i].value;
    }
    return out;
  }

  std::vector<Node> parse_children(size_t depth, bool nested) {
    if (depth > max_depth_) throw ParseError("XMQ nesting limit");
    std::vector<Node> result;
    while (pos_ < tokens_.size()) {
      while (pos_ < tokens_.size() && tokens_[pos_].value == "\n") ++pos_;
      if (pos_ == tokens_.size()) break;
      if (tokens_[pos_].value == "}") {
        if (!nested) error("unexpected closing brace", tokens_[pos_].line);
        ++pos_;
        return result;
      }
      const size_t start_line = tokens_[pos_].line;
      std::vector<Token> header;
      int parens = 0;
      while (pos_ < tokens_.size()) {
        const auto &t = tokens_[pos_];
        if (!t.quoted && t.value == "(") ++parens;
        if (!t.quoted && t.value == ")") --parens;
        if (parens < 0) error("unbalanced parentheses", t.line);
        if (parens == 0 && !t.quoted &&
            (t.value == "\n" || t.value == "{" || t.value == "}")) break;
        header.push_back(t);
        ++pos_;
      }
      if (header.empty()) error("empty statement", start_line);
      if (parens != 0) error("unbalanced parentheses", start_line);
      if (++nodes_ > max_nodes_) throw ParseError("XMQ node limit");
      // Upstream 'template(id = foo)' sometimes places '{' on the next line.
      size_t look = pos_;
      while (look < tokens_.size() && tokens_[look].value == "\n") ++look;
      const bool is_block = look < tokens_.size() && tokens_[look].value == "{";
      Node node;
      node.block = is_block;
      if (is_block) {
        node.key = join(header, 0, header.size());
        pos_ = look + 1;
        node.children = parse_children(depth + 1, true);
      } else {
        size_t equals = header.size();
        for (size_t i = 0, nesting = 0; i < header.size(); ++i) {
          if (header[i].value == "(") ++nesting;
          if (header[i].value == ")" && nesting) --nesting;
          if (!nesting && !header[i].quoted && header[i].value == "=") {
            equals = i; break;
          }
        }
        if (equals == 0 || equals == header.size())
          error("expected key = value or named block", start_line);
        node.key = join(header, 0, equals);
        node.value = join(header, equals + 1, header.size());
        if (node.value.empty()) error("missing value", start_line);
      }
      result.push_back(std::move(node));
    }
    if (nested) throw ParseError("XMQ unclosed block");
    return result;
  }
};

}  // namespace xmq_runtime

#ifdef XMQ_READER_RESTORE_ERROR_MACRO
#pragma pop_macro("error")
#undef XMQ_READER_RESTORE_ERROR_MACRO
#endif
