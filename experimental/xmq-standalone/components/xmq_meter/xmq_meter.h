#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/wmbus_radio/component.h"
#include "standalone_engine.h"
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace esphome { namespace xmq_meter {
class Meter : public Component {
 public:
  void set_meter(uint32_t id, bool heat) { this->meter_id_ = id; this->heat_ = heat; }
  void set_radio(wmbus_radio::Radio *radio);
  void add_sensor(std::string name, sensor::Sensor *s) {
    sensors_.emplace_back(std::move(name), s);
  }
  void dump_config() override {
    ESP_LOGCONFIG("xmq_meter", "XMQ primary meter family=%s, ID=REDACTED", heat_ ? "heat" : "water");
  }
 protected:
  void handle_frame_(wmbus_radio::Frame *frame);
  uint32_t meter_id_{0};
  bool heat_{false};
  std::vector<std::pair<std::string, sensor::Sensor *>> sensors_;
};
}} // namespace esphome::xmq_meter
