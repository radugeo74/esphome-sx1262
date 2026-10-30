#pragma once
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include <utility>
#include <vector>


#include "esphome/components/wmbus_common/meters.h"
#include "esphome/components/wmbus_radio/component.h"

namespace esphome {
namespace wmbus_meter {
class Meter : public Component {
public:
  void set_meter_params(std::string id, std::string driver, std::string key,
                        std::initializer_list<LinkMode> linkModes);
  void set_radio(wmbus_radio::Radio *radio);
  void set_xmq_shadow(bool enabled) { this->xmq_shadow_enabled_ = enabled; }
  void set_xmq_only(bool enabled) { this->xmq_only_enabled_ = enabled; }

  void dump_config() override;
  std::string get_id();
  std::string get_driver();
  std::string get_key();

  void on_telegram(std::function<void()> &&callback);

  std::string as_json(bool pretty_print = false);
  optional<std::string> get_string_field(std::string field_name);
  optional<float> get_numeric_field(std::string field_name);

protected:
  bool xmq_shadow_enabled_{false};
  bool xmq_only_enabled_{false};
  std::vector<std::pair<std::string, double>> xmq_values_;
  uint32_t xmq_shadow_success_{0};
  uint32_t xmq_shadow_failed_{0};
  uint32_t xmq_field_parity_checks_{0};
  uint32_t xmq_field_parity_equal_{0};
  uint32_t xmq_field_parity_different_{0};
  uint32_t xmq_field_parity_missing_xmq_{0};
  uint32_t xmq_field_parity_missing_legacy_{0};
  uint32_t pure_raw_seen_{0};
  uint32_t pure_raw_match_{0};
  uint32_t pure_raw_mismatch_{0};
  uint32_t pure_transport_seen_{0};
  uint32_t pure_plain_seen_{0};
  uint32_t pure_mode7_seen_{0};
  uint32_t pure_proprietary_seen_{0};
  uint32_t pure_unsupported_seen_{0};
  uint32_t pure_mode5_seen_{0};
  uint32_t pure_mode5_ready_{0};
  LinkModeSet link_modes_;
  wmbus_radio::Radio *radio;

  std::shared_ptr<::Meter> meter;
  std::unique_ptr<Telegram> last_telegram;

  CallbackManager<void()> on_telegram_callback_manager;

  void handle_frame(wmbus_radio::Frame *frame);
};
} // namespace wmbus_meter
} // namespace esphome