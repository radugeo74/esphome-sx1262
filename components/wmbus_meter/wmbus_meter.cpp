#include "wmbus_meter.h"
#include "esphome/components/wmbus_radio/pure_frame_header.h"
#include "esphome/components/wmbus_radio/pure_meter_match.h"
#include "esphome/components/wmbus_radio/pure_transport.h"
#include "esphome/components/wmbus_radio/pure_security.h"
#include "esphome/components/wmbus_radio/pure_mode5_prepare.h"
#if defined(WMBUS_XMQ_SHADOW_ENABLED) && !__has_include("xmq_generated.h")
#error "XMQ shadow is enabled but generated tables are missing from ESPHome build"
#endif
#if __has_include("xmq_generated.h")
#include "frame_bridge.h"
#include "xmq_generated.h"
#include "xmq_driver_registry.h"
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

  // Migration diagnostic: independently match the configured meter ID.
  // The legacy handler still owns processing and HA updates in this stage.
  wmbus_pure::Header pure_header{};
  const auto &raw = frame->data();
  const auto header_result =
      wmbus_pure::parse_header(raw.data(), raw.size(), pure_header);
  if (header_result == wmbus_pure::HeaderStatus::Ok) {
#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
    // Diagnostic only: selection is based on generated XMQ detect.mvt rules,
    // not the legacy driver. Does not affect dispatch or published values.
    const auto selection = wmbus_pure::select_driver(
        wmbus_pure::generated::signatures,
        wmbus_pure::generated::signature_count,
        pure_header.manufacturer, pure_header.version, pure_header.device_type);
    if (selection.status == wmbus_pure::DriverSelectionStatus::Found) {
      ESP_LOGV(TAG, "Pure XMQ select meter=%08u driver=%s",
               static_cast<unsigned>(pure_header.meter_id), selection.driver);
    } else {
      ESP_LOGD(TAG, "Pure XMQ selection meter=%08u status=%u",
               static_cast<unsigned>(pure_header.meter_id),
               static_cast<unsigned>(selection.status));
    }
#endif
    const std::string configured_id = this->get_id();
    const auto match = wmbus_pure::match_meter_id(pure_header, configured_id.c_str());
    if (match == wmbus_pure::MatchStatus::InvalidConfiguredId) {
      ESP_LOGD(TAG, "Pure ID matcher unsupported configured meter ID format");
    } else if (match == wmbus_pure::MatchStatus::Match) {
      ESP_LOGD(TAG, "Pure ID match for meter %s", configured_id.c_str());
      // Observe only validated frame layouts. These counters deliberately do
      // not access meter keys, decrypt bytes, or alter legacy dispatch.
      wmbus_pure::TransportEnvelope env{};
      const auto transport_status = wmbus_pure::inspect_transport(pure_header, env);
      if (transport_status == wmbus_pure::TransportStatus::Ok) {
        const auto security = wmbus_pure::inspect_tpl_security(env);
        if (security.kind == wmbus_pure::SecurityKind::AesCbcIvMode5) {
          wmbus_pure::Mode5Prepared prepared{};
          const auto prep = wmbus_pure::prepare_mode5(
              raw.data(), raw.size(), pure_header, env, prepared);
          ++this->pure_mode5_seen_;
          if (prep == wmbus_pure::Mode5PrepareStatus::Ok)
            ++this->pure_mode5_ready_;
          if (this->pure_mode5_seen_ % 30 == 1) {
            ESP_LOGI(TAG, "Pure mode5 diagnostic seen=%u ready=%u prep_status=%u",
                     static_cast<unsigned>(this->pure_mode5_seen_),
                     static_cast<unsigned>(this->pure_mode5_ready_),
                     static_cast<unsigned>(prep));
          }
        }
      }
    }
  }

  auto about =
      AboutTelegram(App.get_friendly_name(), frame->rssi(), FrameType::WMBUS);

  std::vector<Address> adresses;
  bool id_match = false;
  auto telegram = std::make_unique<Telegram>();

  this->meter->handleTelegram(about, frame->data(), false, &adresses, &id_match,
                              telegram.get());

  if (id_match && this->xmq_shadow_enabled_) {
    // Never reuse values from a previous telegram in authoritative mode.
    if (this->xmq_only_enabled_) this->xmq_values_.clear();
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
        // The fhkvdataiii grammar accepts 01, 11 and 0F prefixes.
        // A missing mfct_0f_index must not reject the 01/11 variants.
        // For those, try the bounded parsed application span; the grammar
        // itself still validates the prefix, and this remains shadow-only.
        const auto status = heat && telegram->mfct_0f_index >= 0
            ? decode_manufacturer_frame(heat_tables, bytes, length,
                static_cast<size_t>(telegram->header_size),
                static_cast<size_t>(telegram->suffix_size),
                telegram->mfct_0f_index, captures, 12, decoded)
            : decode_parsed_frame(heat ? heat_tables : water_tables, bytes, length,
                static_cast<size_t>(telegram->header_size),
                static_cast<size_t>(telegram->suffix_size),
                captures, 12, decoded);
        if (status == DriverStatus::Ok) {
          if (this->xmq_only_enabled_) {
            this->xmq_values_.clear();
            for (size_t i = 0; i < decoded.value_count; ++i)
              this->xmq_values_.emplace_back(decoded.values[i].name, decoded.values[i].value);
          }
          ++this->xmq_shadow_success_;
        } else {
          if (this->xmq_only_enabled_) this->xmq_values_.clear();
          ++this->xmq_shadow_failed_;
        }
        // Rate-limited counts; no decrypted payload or encryption keys in logs.
        if ((this->xmq_shadow_success_ + this->xmq_shadow_failed_) % 30 == 1) {
          ESP_LOGI(TAG, "XMQ shadow driver=%s ok=%u fail=%u last_status=%u",
                   name.c_str(), this->xmq_shadow_success_,
                   this->xmq_shadow_failed_, static_cast<unsigned>(status));
          if (status == DriverStatus::BadArguments) {
            PayloadSpan span{};
            const PayloadStatus payload_status = heat
                ? manufacturer_payload(bytes, length,
                    static_cast<size_t>(telegram->header_size),
                    static_cast<size_t>(telegram->suffix_size),
                    telegram->mfct_0f_index, span)
                : application_payload(bytes, length,
                    static_cast<size_t>(telegram->header_size),
                    static_cast<size_t>(telegram->suffix_size), span);
            ESP_LOGI(TAG, "XMQ badargs driver=%s payload_status=%u frame_len=%u header=%d suffix=%d mfct_0f_index=%d",
                     name.c_str(), static_cast<unsigned>(payload_status),
                     static_cast<unsigned>(length),
                     telegram->header_size, telegram->suffix_size,
                     telegram->mfct_0f_index);
          }
          // Log only decoded numeric fields, never keys, raw frames or captures.
          // Limit both frequency and field count to keep reception responsive.
          if (status == DriverStatus::Ok) {
            // The decoder has a fixed 24-value output buffer. Log all values
            // on the existing rate-limited sample so HCA/temperature fields
            // appearing after the date fields are visible for comparison.
            for (size_t i = 0; i < decoded.value_count && i < 24; ++i) {
              ESP_LOGI(TAG, "XMQ value %s=%.6f",
                       decoded.values[i].name, decoded.values[i].value);
            }
          }
        }
      }
    }
#else
    ESP_LOGW(TAG, "XMQ shadow enabled but generated driver tables are missing");
#endif
  }

  if (id_match && (!this->xmq_only_enabled_ || !this->xmq_values_.empty())) {
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

  if (this->xmq_only_enabled_) {
    // This path is authoritative for numeric measurement values.
    // The upstream meter still identifies/decrypts the telegram, but its
    // decoded measurement values are never read by HA sensors.
    std::string wanted = field_name;
    // Water XMQ names have no unit suffix; heat names carry _hca/_c.
    if (wanted == "target_m3") wanted = "target";
    else if (wanted == "total_m3") wanted = "total";
    for (const auto &entry : this->xmq_values_)
      if (entry.first == wanted) return static_cast<float>(entry.second);
    return {};
  }

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
