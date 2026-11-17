#include "wmbus_meter.h"
#include "esphome/components/xmq_shadow_probe/xmq_live_capture_hook.h"

namespace esphome {
namespace wmbus_meter {
static const char *TAG = "wmbus_meter";

void Meter::set_meter_params(std::string id, std::string driver,
                             std::string key,
                             std::initializer_list<LinkMode> linkModes) {
  MeterInfo meter_info;
  meter_info.parse(driver + '-' + id, driver, id + ",", key);

  this->meter = createMeter(&meter_info);

  for (auto linkMode : linkModes)
    this->link_modes_.addLinkMode(linkMode);
}
void Meter::set_radio(wmbus_radio::Radio *radio) {
  this->radio = radio;
  radio->add_meter_frame_handler(
      this->meter->addressExpressions(),
      [this](wmbus_radio::Frame *frame) { this->handle_frame(frame); });
}
void Meter::dump_config() {
  std::string id = this->get_id();
  std::string driver = this->get_driver();
  // Never print an AES key in ESPHome logs (including DEBUG logs).
  const bool has_key = this->get_key() != "not-encrypted";

  ESP_LOGCONFIG(TAG, "wM-Bus Meter:");
  ESP_LOGCONFIG(TAG, "  ID: 0x%s", id.c_str());
  ESP_LOGCONFIG(TAG, "  Driver: %s", driver.c_str());
  ESP_LOGCONFIG(TAG, "  Key: %s", has_key ? "configured (redacted)" : "not configured");
}

std::string Meter::get_id() {
  std::vector<AddressExpression> address_expressions =
      this->meter->addressExpressions();
  return address_expressions.size() > 0 ? address_expressions[0].id : "unknown";
}

std::string Meter::get_driver() { return this->meter->driverName().str(); }

std::string Meter::get_key() {
  MeterKeys *keys = this->meter->meterKeys();
  return keys->hasConfidentialityKey() ? bin2hex(keys->confidentiality_key)
                                       : "not-encrypted";
}

void Meter::handle_frame(wmbus_radio::Frame *frame) {
  if (!this->link_modes_.has(frame->link_mode())) {
    ESP_LOGW(TAG, "Frame link mode %s not supported by meter %s",
             toString(frame->link_mode()), this->meter->name().c_str());
    return;
  }

  auto about =
      AboutTelegram(App.get_friendly_name(), frame->rssi(), FrameType::WMBUS);

  std::vector<Address> adresses;
  bool id_match = false;
  auto telegram = std::make_unique<Telegram>();

  // Baseline values are observed BEFORE legacy decoding. Comparing before/after
  // can prove a value changed, but equality cannot prove per-frame provenance.
  // Keep the old decoder, radio dispatch and sensor updates unchanged.
  xmq_shadow_probe::LegacyReadings before;
  xmq_shadow_probe::LegacyReadings legacy;
  const bool probe_requested = xmq_shadow_probe::wants_legacy_readings();
  const std::string shadow_driver = probe_requested ? this->get_driver() : "";
  auto capture = [this](const char *name,
                        xmq_shadow_probe::LegacyNumeric &dest) {
    const auto value = this->get_numeric_field(name);
    if (value.has_value() && std::isfinite(*value)) {
      dest.value = *value;
      dest.present = true;
    }
  };
  auto date = [this](const char *name) -> std::string {
    auto *field = this->meter->findFieldInfo(name, Quantity::PointInTime);
    if (!field) return {};
    const std::string text = this->meter->getStringValue(field);
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return {};
    for (size_t i = 0; i < text.size(); ++i)
      if (i != 4 && i != 7 && (text[i] < '0' || text[i] > '9')) return {};
    return text;
  };
  auto snapshot = [&](xmq_shadow_probe::LegacyReadings &readings) {
    if (shadow_driver == "mkradio4") {
      capture("target_m3", readings.target_m3);
      capture("total_m3", readings.total_m3);
    } else if (shadow_driver == "fhkvdataiii") {
      capture("current_hca", readings.current_hca);
      capture("previous_hca", readings.previous_hca);
      capture("temp_room_c", readings.temp_room_c);
      capture("temp_radiator_c", readings.temp_radiator_c);
      readings.current_date = date("current");
      readings.previous_date = date("previous");
    }
  };
  if (probe_requested) snapshot(before);

  const bool decoded = this->meter->handleTelegram(
      about, frame->data(), false, &adresses, &id_match, telegram.get());

  if (decoded && probe_requested) {
    snapshot(legacy);
    // This telegram records which FieldInfo extractors were selected by
    // parsed DIF/VIF entries. This is supporting evidence of an attempted
    // per-frame extraction, not proof of a successful field write.
    auto matched_dv = [&](const char *name, Quantity quantity) {
      auto *field = this->meter->findFieldInfo(name, quantity);
      if (field == nullptr) return false;
      for (auto &entry : telegram->dv_entries)
        if (entry.second.second.hasFieldInfo(field)) return true;
      return false;
    };
    auto evidence = [&](const char *field_name,
                        xmq_shadow_probe::LegacyNumeric &value) {
      if (!value.present) return;
      std::string name;
      Unit unit;
      extractUnit(field_name, &name, &unit);
      value.matched_dv = matched_dv(name.c_str(), toQuantity(unit));
    };
    evidence("target_m3", legacy.target_m3);
    evidence("total_m3", legacy.total_m3);
    evidence("current_hca", legacy.current_hca);
    evidence("previous_hca", legacy.previous_hca);
    evidence("temp_room_c", legacy.temp_room_c);
    evidence("temp_radiator_c", legacy.temp_radiator_c);
    legacy.current_date_matched_dv =
        !legacy.current_date.empty() &&
        matched_dv("current", Quantity::PointInTime);
    legacy.previous_date_matched_dv =
        !legacy.previous_date.empty() &&
        matched_dv("previous", Quantity::PointInTime);
    auto mark = [](xmq_shadow_probe::LegacyNumeric &after,
                   const xmq_shadow_probe::LegacyNumeric &prior) {
      after.changed = after.present &&
                      (!prior.present || after.value != prior.value);
    };
    mark(legacy.target_m3, before.target_m3);
    mark(legacy.total_m3, before.total_m3);
    mark(legacy.current_hca, before.current_hca);
    mark(legacy.previous_hca, before.previous_hca);
    mark(legacy.temp_room_c, before.temp_room_c);
    mark(legacy.temp_radiator_c, before.temp_radiator_c);
    legacy.current_date_changed =
        !legacy.current_date.empty() &&
        legacy.current_date != before.current_date;
    legacy.previous_date_changed =
        !legacy.previous_date.empty() &&
        legacy.previous_date != before.previous_date;
  }
  xmq_shadow_probe::observe_legacy_telegram(
      *telegram, decoded, shadow_driver.c_str(), legacy);

  if (id_match) {
    this->last_telegram = std::move(telegram);
    this->defer([this]() {
      this->on_telegram_callback_manager();
      this->last_telegram = nullptr;
    });

    frame->mark_as_handled();
  }
}

std::string Meter::as_json(bool pretty_print) {
  std::string json;
  this->meter->printMeter(this->last_telegram.get(), nullptr, nullptr, '\t',
                          &json, nullptr, nullptr, nullptr, pretty_print);
  return json;
}

optional<std::string> Meter::get_string_field(std::string field_name) {

  if (field_name == "timestamp")
    return this->meter->datetimeOfUpdateHumanReadable();

  if (field_name == "timestamp_zulu")
    return this->meter->datetimeOfUpdateRobot();

  auto field_info = this->meter->findFieldInfo(field_name, Quantity::Text);
  if (field_info)
    return this->meter->getStringValue(field_info);

  return {};
}

optional<float> Meter::get_numeric_field(std::string field_name) {
  // RSSI is not handled by meter but by telegram :/
  if (field_name == "rssi_dbm")
    return this->last_telegram->about.rssi_dbm;

  if (field_name == "timestamp")
    return this->meter->timestampLastUpdate();

  std::string name;
  Unit unit;
  extractUnit(field_name, &name, &unit);

  auto value = this->meter->getNumericValue(name, unit);

  if (!std::isnan(value))
    return value;

  return {};
}

void Meter::on_telegram(std::function<void()> &&callback) {
  this->on_telegram_callback_manager.add(std::move(callback));
}

} // namespace wmbus_meter
} // namespace esphome
