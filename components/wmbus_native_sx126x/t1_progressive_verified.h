#pragma once
// Progressive T1 assembly -> complete DLL verification, fail-closed.
// Radio-independent; SX126x still needs an explicit FIFO callback/adapter.
#include "t1_progressive_buffer.h"
#include "native_verified_frame.h"

namespace esphome {
namespace wmbus_native_sx126x {
namespace t1 {
enum class VerifiedFeedStatus : uint8_t { NeedMore, Verified, Rejected };
class ProgressiveVerified {
 public:
  void reset() { this->buffer_.reset(); this->verified_ = {}; this->finished_ = false; }
  VerifiedFeedStatus feed(const uint8_t *data, size_t length) {
    if (this->finished_) return VerifiedFeedStatus::Rejected;
    this->verified_ = {};
    const auto status = this->buffer_.feed(data, length);
    if (status == FeedStatus::NeedMore) return VerifiedFeedStatus::NeedMore;
    if (status == FeedStatus::Rejected) { this->finished_ = true; return VerifiedFeedStatus::Rejected; }
    const auto &raw = this->buffer_.bytes();
    this->verified_ = verified::process(boundary::Mode::T1, raw.data(), raw.size());
    this->finished_ = true;
    if (this->verified_.status != verified::Status::Ok)
      return VerifiedFeedStatus::Rejected;
    return VerifiedFeedStatus::Verified;
  }
  const verified::Result &result() const { return this->verified_; }
  const std::vector<uint8_t> &raw_encoded_bytes() const { return this->buffer_.bytes(); }
 private:
  ProgressiveBuffer buffer_{};
  verified::Result verified_{};
  bool finished_{false};
};
}  // namespace t1
}  // namespace wmbus_native_sx126x
}  // namespace esphome
