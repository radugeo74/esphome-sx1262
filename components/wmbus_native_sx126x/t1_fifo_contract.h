#pragma once
// Non-owning integration contract for a future official SX126x progressive FIFO hook.
// The radio owner calls begin_frame(), feed_bytes() and end_frame() in order.
// No SPI, IRQ, GPIO or SX126x driver calls are implemented here.
#include <cstddef>
#include <cstdint>
#include "t1_progressive_verified.h"

namespace esphome {
namespace wmbus_native_sx126x {
namespace t1 {
enum class RxState : uint8_t { Idle, Collecting, Verified, Rejected };
class FifoContract {
 public:
  void begin_frame() { receiver_.reset(); state_ = RxState::Collecting; }
  RxState feed_bytes(const uint8_t *bytes, size_t size) {
    if (state_ != RxState::Collecting) return state_;
    switch (receiver_.feed(bytes, size)) {
      case VerifiedFeedStatus::NeedMore: break;
      case VerifiedFeedStatus::Verified: state_ = RxState::Verified; break;
      case VerifiedFeedStatus::Rejected: state_ = RxState::Rejected; break;
    }
    return state_;
  }
  RxState end_frame() {
    if (state_ == RxState::Collecting) state_ = RxState::Rejected;
    return state_;
  }
  RxState state() const { return state_; }
  const verified::Result &result() const { return receiver_.result(); }
 private:
  ProgressiveVerified receiver_{};
  RxState state_{RxState::Idle};
};
}  // namespace t1
}  // namespace wmbus_native_sx126x
}  // namespace esphome
