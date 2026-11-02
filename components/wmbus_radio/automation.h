#pragma once

#include "component.h"
#include "esphome/core/automation.h"
#include "packet.h"

namespace esphome {
namespace wmbus_radio {
class FrameTrigger : public Trigger<Frame *> {
public:
  explicit FrameTrigger(wmbus_radio::Radio *radio, bool mark_handled) {
    radio->add_frame_handler([this, mark_handled](Frame *frame) {
      this->trigger(frame);
      if (mark_handled)
        frame->mark_as_handled();
    });
  }
};

// Discovery hooks report raw frames; they never mark a frame as decoded.
class DiscoveryFrameTrigger : public Trigger<Frame *> {
public:
  explicit DiscoveryFrameTrigger(Radio *radio) {
    radio->add_discovery_handler([this](Frame *frame) { this->trigger(frame); });
  }
};

class UnverifiedFrameTrigger : public Trigger<Frame *> {
public:
  explicit UnverifiedFrameTrigger(Radio *radio) {
    radio->add_unverified_handler([this](Frame *frame) { this->trigger(frame); });
  }
};

} // namespace wmbus_radio
} // namespace esphome