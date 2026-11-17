#pragma once
// Host-testable per-telegram write journal for a future isolated legacy
// common-decoder adapter. No coupling to ESPHome, radio, or global state.
// IMPORTANT: this class is NOT connected to live setNumericValue calls yet.
#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>

namespace xmq_runtime {

struct LegacyWriteJournal {
  struct Counts {
    std::size_t numeric_writes{0};
    std::size_t string_writes{0};
    std::size_t distinct_numeric{0};
    std::size_t distinct_string{0};
  };

  explicit LegacyWriteJournal(std::size_t max_fields = 32,
                              std::size_t max_writes = 128)
      : max_fields_(max_fields), max_writes_(max_writes) {
    if (max_fields_ == 0 || max_writes_ == 0)
      throw std::invalid_argument("invalid write journal bounds");
  }

  void begin_telegram() {
    numeric_.clear();
    strings_.clear();
    counts_ = {};
    active_ = true;
  }

  void end_telegram() noexcept { active_ = false; }

  void numeric_write(const std::string &name) {
    record(name, numeric_, counts_.numeric_writes,
           counts_.distinct_numeric);
  }

  void string_write(const std::string &name) {
    record(name, strings_, counts_.string_writes,
           counts_.distinct_string);
  }

  // A per-telegram write is observable even if the written value is
  // numerically equal to the previous telegram's value.
  bool wrote_numeric(const std::string &name) const {
    return numeric_.find(name) != numeric_.end();
  }

  bool wrote_string(const std::string &name) const {
    return strings_.find(name) != strings_.end();
  }

  bool active() const noexcept { return active_; }
  const Counts &counts() const noexcept { return counts_; }

private:
  void record(const std::string &name, std::map<std::string, bool> &fields,
              std::size_t &writes, std::size_t &distinct) {
    if (!active_)
      throw std::logic_error("write outside telegram");
    if (name.empty() || name.size() > 96)
      throw std::invalid_argument("invalid field name");
    if (writes >= max_writes_)
      throw std::length_error("too many field writes");
    if (fields.find(name) == fields.end()) {
      if (numeric_.size() + strings_.size() >= max_fields_)
        throw std::length_error("too many tracked fields");
      fields.emplace(name, true);
      ++distinct;
    }
    ++writes;
  }

  const std::size_t max_fields_;
  const std::size_t max_writes_;
  bool active_{false};
  Counts counts_{};
  std::map<std::string, bool> numeric_;
  std::map<std::string, bool> strings_;
};

}  // namespace xmq_runtime
