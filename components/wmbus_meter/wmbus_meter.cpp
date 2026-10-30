#include "wmbus_meter.h"
#include <cmath>
#include <cstring>
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
#include "xmq_decode_catalog.h"
#include "xmq_field_parity.h"
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
  radio->add_meter_handler(this->get_id(),
      [this](wmbus_radio::Frame *frame) { this->handle_frame(frame); },
      [this](LinkMode mode) { return this->link_modes_.has(mode); });
}
void Meter::dump_config() {
  std::string id = this->get_id();
  std::string driver = this->get_driver();
  const bool has_key = this->meter->meterKeys()->hasConfidentialityKey();

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

  // Migration diagnostic: independently match the configured meter ID.
  // The legacy handler still owns processing and HA updates in this stage.
  wmbus_pure::Header pure_header{};
  const auto &raw = frame->data();
  const auto header_result =
      wmbus_pure::parse_header(raw.data(), raw.size(), pure_header);
  if (header_result == wmbus_pure::HeaderStatus::Ok) {
    const std::string configured_id = this->get_id();
    const auto match = wmbus_pure::match_meter_id(pure_header, configured_id.c_str());
    if (match == wmbus_pure::MatchStatus::InvalidConfiguredId) {
      ESP_LOGD(TAG, "Pure ID matcher unsupported configured meter ID format");
    } else if (match == wmbus_pure::MatchStatus::Match) {
      ESP_LOGD(TAG, "Pure ID match for meter %s", configured_id.c_str());
#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
      // Shadow-only registry selection on the independently matched meter.
      // Legacy still controls dispatch, decryption and sensor publishing.
      const auto selection = wmbus_pure::select_driver(
          wmbus_pure::generated::signatures,
          wmbus_pure::generated::signature_count,
          pure_header.manufacturer, pure_header.version, pure_header.device_type);
      const std::string legacy_driver = this->get_driver();
      const bool registry_found =
          selection.status == wmbus_pure::DriverSelectionStatus::Found &&
          selection.driver != nullptr;
      const bool registry_match = registry_found && legacy_driver == selection.driver;
      static uint32_t registry_checked = 0;
      static uint32_t registry_equal = 0;
      static uint32_t registry_missing = 0;
      static uint32_t registry_different = 0;
      ++registry_checked;
      if (registry_match) ++registry_equal;
      else if (!registry_found) ++registry_missing;
      else ++registry_different;
      if (registry_checked % 30 == 1 ||
          (!registry_match && registry_missing + registry_different <= 3)) {
        ESP_LOGI(TAG,
                 "Pure registry shadow checked=%u equal=%u missing=%u different=%u meter=%08u selected=%s configured=%s ci=0x%02X",
                 static_cast<unsigned>(registry_checked),
                 static_cast<unsigned>(registry_equal),
                 static_cast<unsigned>(registry_missing),
                 static_cast<unsigned>(registry_different),
                 static_cast<unsigned>(pure_header.meter_id),
                 registry_found ? selection.driver : "none",
                 legacy_driver.c_str(), static_cast<unsigned>(pure_header.ci));
      }
#endif
      // Observe only validated frame layouts. These counters deliberately do
      // not access meter keys, decrypt bytes, or alter legacy dispatch.
      wmbus_pure::TransportEnvelope env{};
      const auto transport_status = wmbus_pure::inspect_transport(pure_header, env);
      if (transport_status == wmbus_pure::TransportStatus::Ok) {
        ++this->pure_transport_seen_;
        const auto security = wmbus_pure::inspect_tpl_security(env);
        if (env.kind == wmbus_pure::TransportKind::ManufacturerSpecific)
          ++this->pure_proprietary_seen_;
        else if (security.kind == wmbus_pure::SecurityKind::NoSecurity)
          ++this->pure_plain_seen_;
        else if (security.kind == wmbus_pure::SecurityKind::AesCbcNoIvMode7)
          ++this->pure_mode7_seen_;
        else if (security.kind == wmbus_pure::SecurityKind::Unsupported)
          ++this->pure_unsupported_seen_;
        if (this->pure_transport_seen_ % 30 == 1) {
          ESP_LOGI(TAG,
                   "Pure TPL diag meter=%s ci=0x%02X kind=%u security_mode=%u "
                   "total=%u plain=%u mode5=%u ready5=%u mode7=%u proprietary=%u unsupported=%u",
                   configured_id.c_str(), static_cast<unsigned>(env.ci),
                   static_cast<unsigned>(env.kind),
                   static_cast<unsigned>(security.mode),
                   static_cast<unsigned>(this->pure_transport_seen_),
                   static_cast<unsigned>(this->pure_plain_seen_),
                   static_cast<unsigned>(this->pure_mode5_seen_),
                   static_cast<unsigned>(this->pure_mode5_ready_),
                   static_cast<unsigned>(this->pure_mode7_seen_),
                   static_cast<unsigned>(this->pure_proprietary_seen_),
                   static_cast<unsigned>(this->pure_unsupported_seen_));
        }
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

  // Independent pre-legacy pipeline. Match/registry/CI and decode the radio
  // payload before legacy Telegram processing. Observation only; never publish.
#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
  bool pure_pipeline_ok = false;
  bool pure_pipeline_attempted = false;
  const char *pure_pipeline_driver = "none";
  size_t pure_pipeline_value_count = 0;
  if (this->xmq_shadow_enabled_ &&
      header_result == wmbus_pure::HeaderStatus::Ok) {
    const std::string configured_id = this->get_id();
    if (wmbus_pure::match_meter_id(pure_header, configured_id.c_str()) ==
        wmbus_pure::MatchStatus::Match) {
      const auto selection = wmbus_pure::select_driver(
          wmbus_pure::generated::signatures,
          wmbus_pure::generated::signature_count,
          pure_header.manufacturer, pure_header.version, pure_header.device_type);
      if (selection.status == wmbus_pure::DriverSelectionStatus::Found &&
          selection.driver != nullptr) {
        pure_pipeline_driver = selection.driver;
        const auto *descriptor = xmq_portable::find_decode_descriptor(
            selection.driver, pure_header.ci);
        if (descriptor) {
          using namespace xmq_portable;
          Capture pure_captures[12]{};
          CalcValue pure_values[24]{};
          CalendarDate pure_dates[2]{};
          DecodedPayload pure_decoded{pure_values, 24, 0, pure_dates, 2, 0};
          pure_pipeline_attempted = true;
          const auto decode_status = decode_driver_payload(
              descriptor->tables, pure_header.application,
              pure_header.application_length,
              pure_captures, 12, pure_decoded);
          pure_pipeline_ok = decode_status == DriverStatus::Ok;
          pure_pipeline_value_count = pure_decoded.value_count;
        }
      }
    }
  }
  if (pure_pipeline_attempted) {
    static uint32_t pipeline_seen = 0;
    static uint32_t pipeline_ok = 0;
    static uint32_t pipeline_failed = 0;
    ++pipeline_seen;
    if (pure_pipeline_ok) ++pipeline_ok;
    else ++pipeline_failed;
    if (pipeline_seen % 30 == 1 || (!pure_pipeline_ok && pipeline_failed <= 3))
      ESP_LOGI(TAG, "Pure pipeline prelegacy seen=%u ok=%u fail=%u driver=%s values=%u",
               static_cast<unsigned>(pipeline_seen),
               static_cast<unsigned>(pipeline_ok),
               static_cast<unsigned>(pipeline_failed),
               pure_pipeline_driver,
               static_cast<unsigned>(pure_pipeline_value_count));
  }
#endif

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
      const auto *descriptor = xmq_portable::find_decode_descriptor(
          name.c_str(), pure_header.ci);
      if (descriptor) {
        using namespace xmq_portable;
        Capture captures[12]{};
        CalcValue values[24]{};
        CalendarDate dates[2]{};
        DecodedPayload decoded{values, 24, 0, dates, 2, 0};
        const auto *bytes = telegram->frame.data();
        const size_t length = telegram->frame.size();
        // Catalog policy preserves legacy 0F offset handling and 01/11 fallback.
        const auto status = decode_descriptor_frame(
            *descriptor, bytes, length,
            static_cast<size_t>(telegram->header_size),
            static_cast<size_t>(telegram->suffix_size),
            telegram->mfct_0f_index, captures, 12, decoded);
        // Independent shadow: start at DLL bytes (already CRC stripped by
        // radio), parse the header ourselves and pass the manufacturer
        // application payload straight to the portable XMQ decoder.
        // Never use Telegram header_size, mfct_0f_index or decrypted content.
        // CI=A0 (Techem HCA) and CI=A2 (Techem water) are opaque payloads.
        wmbus_pure::Header independent{};
        if (wmbus_pure::parse_header(raw.data(), raw.size(), independent)
                  == wmbus_pure::HeaderStatus::Ok &&
            independent.meter_id == pure_header.meter_id &&
            independent.ci == descriptor->ci) {
          Capture independent_captures[12]{};
          CalcValue independent_values[24]{};
          CalendarDate independent_dates[2]{};
          DecodedPayload independent_decoded{
              independent_values, 24, 0, independent_dates, 2, 0};
          const auto independent_status = decode_driver_payload(
              descriptor->tables,
              independent.application, independent.application_length,
              independent_captures, 12, independent_decoded);
          ++this->pure_raw_seen_;
          bool same = independent_status == DriverStatus::Ok &&
                      status == DriverStatus::Ok &&
                      independent_decoded.value_count == decoded.value_count;
          if (same) {
            for (size_t j = 0; j < decoded.value_count; ++j) {
              bool found = false;
              for (size_t k = 0; k < independent_decoded.value_count; ++k) {
                if (std::strcmp(decoded.values[j].name,
                                independent_decoded.values[k].name) == 0 &&
                    std::fabs(decoded.values[j].value -
                              independent_decoded.values[k].value) <= 0.000001) {
                  found = true;
                  break;
                }
              }
              if (!found) { same = false; break; }
            }
          }
          if (same) ++this->pure_raw_match_;
          else ++this->pure_raw_mismatch_;
          if (this->pure_raw_seen_ % 30 == 1 ||
              (!same && this->pure_raw_mismatch_ <= 3)) {
            ESP_LOGI(TAG,
                     "Pure raw XMQ driver=%s ci=0x%02X seen=%u match=%u mismatch=%u status=%u",
                     name.c_str(), independent.ci,
                     static_cast<unsigned>(this->pure_raw_seen_),
                     static_cast<unsigned>(this->pure_raw_match_),
                     static_cast<unsigned>(this->pure_raw_mismatch_),
                     static_cast<unsigned>(independent_status));
          }
        }
        // Compare the measurements actually exposed to HA against the current
        // legacy meter state, after successful legacy telegram handling.
        // Diagnostic only: neither decoder's values are written or published.
        if (status == DriverStatus::Ok && !this->xmq_only_enabled_) {
          size_t field_count = 0;
          const auto *fields = parity_fields(name.c_str(), field_count);
          uint32_t equal = 0, different = 0, missing_xmq = 0, missing_legacy = 0;
          for (size_t i = 0; i < field_count; ++i) {
            std::string legacy_name;
            ::Unit legacy_unit;
            extractUnit(fields[i].legacy_field, &legacy_name, &legacy_unit);
            const double legacy_value = this->meter->getNumericValue(legacy_name, legacy_unit);
            double xmq_value = NAN;
            const auto verdict = compare_numeric_field(
                decoded, fields[i], legacy_value, std::isfinite(legacy_value), xmq_value);
            if (verdict == FieldParity::Equal) ++equal;
            else if (verdict == FieldParity::Different) ++different;
            else if (verdict == FieldParity::MissingXmq) ++missing_xmq;
            else ++missing_legacy;
            if (verdict != FieldParity::Equal &&
                (this->xmq_field_parity_checks_ < 4)) {
              ESP_LOGW(TAG, "XMQ field parity meter=%s field=%s result=%u xmq=%.6f legacy=%.6f",
                       this->get_id().c_str(), fields[i].legacy_field,
                       static_cast<unsigned>(verdict), xmq_value, legacy_value);
            }
          }
          ++this->xmq_field_parity_checks_;
          this->xmq_field_parity_equal_ += equal;
          this->xmq_field_parity_different_ += different;
          this->xmq_field_parity_missing_xmq_ += missing_xmq;
          this->xmq_field_parity_missing_legacy_ += missing_legacy;
          if (this->xmq_field_parity_checks_ % 30 == 1 ||
              (different + missing_xmq + missing_legacy && this->xmq_field_parity_checks_ <= 4)) {
            ESP_LOGI(TAG, "XMQ field parity meter=%s frames=%u equal=%u different=%u missing_xmq=%u missing_legacy=%u",
                     this->get_id().c_str(),
                     static_cast<unsigned>(this->xmq_field_parity_checks_),
                     static_cast<unsigned>(this->xmq_field_parity_equal_),
                     static_cast<unsigned>(this->xmq_field_parity_different_),
                     static_cast<unsigned>(this->xmq_field_parity_missing_xmq_),
                     static_cast<unsigned>(this->xmq_field_parity_missing_legacy_));
          }
        }
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
            const PayloadStatus payload_status = descriptor_payload(
                *descriptor, bytes, length,
                static_cast<size_t>(telegram->header_size),
                static_cast<size_t>(telegram->suffix_size),
                telegram->mfct_0f_index, span);
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
