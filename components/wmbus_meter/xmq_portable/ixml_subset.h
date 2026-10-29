#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace xmq_portable {
// Bounded subset of IXML byte/word grammar. Rule tree is generated from XMQ
// definitions; this interpreter has no meter-specific byte offsets.
enum class Op : uint8_t { Byte, Word, Literal, Capture, Rest, Call, Choice };
struct Step { Op op; uint16_t arg; const char *name; };
struct Rule { const char *name; const Step *steps; size_t count; };
struct CaptureKey { const char *capture; const char *difvif; };
struct FieldKey { const char *field; const char *difvif; };
struct Capture { const char *name; const uint8_t *data; size_t length; };
struct Result { Capture *captures; size_t capacity; size_t count; size_t consumed; };
enum class ParseStatus : uint8_t { Ok, InvalidGrammar, Truncated, NoMatch, CapacityExceeded, DepthExceeded };
inline const Rule *find_rule(const Rule *rules, size_t count, const char *name) {
  if (!name) return nullptr;
  for (size_t i = 0; i < count; ++i)
    if (std::strcmp(rules[i].name, name) == 0) return &rules[i];
  return nullptr;
}
inline ParseStatus execute(const Rule *rules, size_t count, const char *entry,
                           const uint8_t *data, size_t length, Result &result,
                           unsigned max_depth = 24) {
  if (!rules || !entry || !data || !result.captures || result.capacity == 0)
    return ParseStatus::InvalidGrammar;
  result.count = 0;
  result.consumed = 0;
  size_t pos = 0, last_begin = 0, last_length = 0;
  auto call = [&](auto &&self, const Rule *rule, unsigned depth) -> ParseStatus {
    if (!rule) return ParseStatus::InvalidGrammar;
    if (depth > max_depth) return ParseStatus::DepthExceeded;
    for (size_t i = 0; i < rule->count; ++i) {
      const Step &s = rule->steps[i];
      switch (s.op) {
        case Op::Byte: case Op::Word: {
          const size_t size = s.op == Op::Byte ? 1 : 2;
          if (size > length - pos) return ParseStatus::Truncated;
          last_begin = pos; last_length = size; pos += size;
          break;
        }
        case Op::Literal:
          if (pos >= length || data[pos] != static_cast<uint8_t>(s.arg))
            return ParseStatus::NoMatch;
          ++pos;
          break;
        case Op::Capture:
          if (!s.name || !last_length) return ParseStatus::InvalidGrammar;
          if (result.count >= result.capacity) return ParseStatus::CapacityExceeded;
          result.captures[result.count++] = {s.name, data + last_begin, last_length};
          break;
        case Op::Rest:
          last_begin = pos; last_length = length - pos; pos = length;
          break;

        case Op::Choice: {
          if (!s.name) return ParseStatus::InvalidGrammar;
          const char *cursor = s.name;
          ParseStatus failure = ParseStatus::NoMatch;
          bool matched = false;
          while (*cursor) {
            const char *end = std::strchr(cursor, '|');
            const size_t size = end ? static_cast<size_t>(end - cursor) : std::strlen(cursor);
            if (!size || size >= 64) return ParseStatus::InvalidGrammar;
            char candidate[64]{};
            std::memcpy(candidate, cursor, size);
            const auto old_pos = pos;
            const auto old_count = result.count;
            const auto old_begin = last_begin;
            const auto old_length = last_length;
            const auto status = self(self, find_rule(rules, count, candidate), depth + 1);
            if (status == ParseStatus::Ok) { matched = true; break; }
            if (status != ParseStatus::NoMatch && status != ParseStatus::Truncated)
              return status;
            failure = status;
            pos = old_pos;
            result.count = old_count;
            last_begin = old_begin;
            last_length = old_length;
            if (!end) break;
            cursor = end + 1;
          }
          if (!matched) return failure;
          break;
        }
        case Op::Call: {
          const auto status = self(self, find_rule(rules, count, s.name), depth + 1);
          if (status != ParseStatus::Ok) return status;
          break;
        }
      }
    }
    return ParseStatus::Ok;
  };
  const ParseStatus status = call(call, find_rule(rules, count, entry), 0);
  result.consumed = pos;
  if (status == ParseStatus::Ok && pos != length) return ParseStatus::NoMatch;
  return status;
}
}  // namespace xmq_portable
