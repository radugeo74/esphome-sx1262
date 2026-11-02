#pragma once
// Pure, bounded T1 FIFO collector. No SX126x register access or publishing.
#include <cstddef>
#include <cstdint>
#include <vector>
#include "t1_packet_framing.h"

namespace esphome {
namespace wmbus_native_sx126x {
namespace t1 {
enum class FeedStatus : uint8_t { NeedMore, Complete, Rejected };
class ProgressiveBuffer {
 public:
  void reset() { bytes_.clear(); expected_ = 0; rejected_ = false; }
  FeedStatus feed(const uint8_t *data, size_t size) {
    if (rejected_) return FeedStatus::Rejected;
    if (!data && size) return this->reject_();
    if (bytes_.size() + size > MAX_ENCODED) return this->reject_();
    if (size != 0) bytes_.insert(bytes_.end(), data, data + size);
    if (expected_ == 0) {
      size_t required = 0;
      const auto status = expected_encoded_size(bytes_.data(), bytes_.size(), required);
      if (status == FrameStatus::Incomplete || status == FrameStatus::Empty)
        return FeedStatus::NeedMore;
      if (status != FrameStatus::Ok) return this->reject_();
      expected_ = required;
    }
    if (bytes_.size() > expected_) return this->reject_();
    return bytes_.size() == expected_ ? FeedStatus::Complete : FeedStatus::NeedMore;
  }
  size_t expected_size() const { return expected_; }
  const std::vector<uint8_t> &bytes() const { return bytes_; }
 private:
  FeedStatus reject_() { rejected_ = true; bytes_.clear(); expected_ = 0; return FeedStatus::Rejected; }
  static constexpr size_t MAX_ENCODED = (255 * 12 + 7) / 8;
  std::vector<uint8_t> bytes_{};
  size_t expected_{0};
  bool rejected_{false};
};
}  // namespace t1
}  // namespace wmbus_native_sx126x
}  // namespace esphome
