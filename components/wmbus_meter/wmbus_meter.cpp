#include "wmbus_meter.h"
#if __has_include("../../experimental/xmq/generated/generated_ixml.h")
#include "../../experimental/xmq/portable_cpp/frame_bridge.h"
#include "../../experimental/xmq/generated/generated_ixml.h"
#define WMBUS_XMQ_TABLES_AVAILABLE 1
#endif

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
  radio->add_frame_handler(
      [this](wmbus_radio::Frame *frame) { return this->handle_frame(frame); });
}
void Meter::dump_config() {
  std::string id = this->get_id();
  std::string driver = this->get_driver();
  std::string key = this->get_key();

  ESP_LOGCONFIG(TAG, "wM-Bus Meter:");
  ESP_LOGCONFIG(TAG, "  ID: 0x%s", id.c_str());
  ESP_LOGCONFIG(TAG, "  Driver: %s", driver.c_str());
  ESP_LOGCONFIG(TAG, "  Key: %s", key.c_str());
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

  this->meter->handleTelegram(about, frame->data(), false, &adresses, &id_match,
                              telegram.get());

  if (id_match && this->xmq_shadow_enabled_) {
#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
    // Observation only: do not call mark_as_handled(), change meter state,
    // or publish any values. Run only after legacy ID matching and decryption.
    if (!telegram->decryption_failed &&
        telegram->header_size >= 0 && telegram->suffix_size >= 0 &&
        !telegram->frame.empty()) {
      const auto name = this->meter->driverName().str();
      const bool heat = name == "fhkvdataiii";
      const bool water = name == "mkradio4";
      if (heat || water) {
        using namespace xmq_portable;
        const DriverTables heat_tables{
          generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
          generated_ixml::fhkvdataiii::capture_keys,
          sizeof(generated_ixml::fhkvdataiii::capture_keys) / sizeof(CaptureKey),
          generated_ixml::fhkvdataiii::field_keys,
          sizeof(generated_ixml::fhkvdataiii::field_keys) / sizeof(FieldKey),
          generated_ixml::fhkvdataiii::calculations,
          generated_ixml::fhkvdataiii::calculation_count,
          generated_ixml::fhkvdataiii::date_calculations,
          generated_ixml::fhkvdataiii::date_calculation_count};
        const DriverTables water_tables{
          generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
          generated_ixml::mkradio4::capture_keys,
          sizeof(generated_ixml::mkradio4::capture_keys) / sizeof(CaptureKey),
          generated_ixml::mkradio4::field_keys,
          sizeof(generated_ixml::mkradio4::field_keys) / sizeof(FieldKey),
          generated_ixml::mkradio4::calculations,
          generated_ixml::mkradio4::calculation_count, nullptr, 0};
        Capture captures[12]{};
        CalcValue values[24]{};
        CalendarDate dates[2]{};
        DecodedPayload decoded{values, 24, 0, dates, 2, 0};
        const auto *bytes = telegram->frame.data();
        const size_t length = telegram->frame.size();
        const auto status = heat
            ? decode_manufacturer_frame(heat_tables, bytes, length,
                static_cast<size_t>(telegram->header_size),
                static_cast<size_t>(telegram->suffix_size),
                telegram->mfct_0f_index, captures, 12, decoded)
            : decode_parsed_frame(water_tables, bytes, length,
                static_cast<size_t>(telegram->header_size),
                static_cast<size_t>(telegram->suffix_size),
                captures, 12, decoded);
        if (status == DriverStatus::Ok) {
          ++this->xmq_shadow_success_;
        } else {
          ++this->xmq_shadow_failed_;
        }
        // Rate-limited counts; no decrypted payload or encryption keys in logs.
        if ((this->xmq_shadow_success_ + this->xmq_shadow_failed_) % 30 == 1)
          ESP_LOGI(TAG, "XMQ shadow driver=%s ok=%u fail=%u last_status=%u",
                   name.c_str(), this->xmq_shadow_success_,
                   this->xmq_shadow_failed_, static_cast<unsigned>(status));
      }
    }
#else
    ESP_LOGW(TAG, "XMQ shadow enabled but generated driver tables are missing");
#endif
  }

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
