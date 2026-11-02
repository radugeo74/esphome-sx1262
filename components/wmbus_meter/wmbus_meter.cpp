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
#include "esphome/components/wmbus_radio/pure_mode5_decrypt.h"
#include "esphome/components/wmbus_radio/pure_aes_mbedtls.h"
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

#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
  // Opt-in, plaintext-only, legacy-free decode and publication path.
  // Preserve the existing radio dispatch and callback API for HA entities.
  if (this->xmq_direct_plain_enabled_) {
    const auto &bytes = frame->data();
    xmq_portable::CalcValue values[24]{};
    xmq_portable::CalendarDate dates[2]{};
    xmq_portable::Capture captures[12]{};
    xmq_portable::DecodedPayload decoded{values, 24, 0, dates, 2, 0};
    const std::string id = this->get_id();
    const std::string driver = this->get_driver();
    const auto status = xmq_portable::decode_configured_plain_frame(
        bytes.data(), bytes.size(), id.c_str(), driver.c_str(),
        captures, 12, decoded);
    this->xmq_values_.clear();
    if (status != xmq_portable::PureDecodeStatus::Ok) {
      ++this->xmq_shadow_failed_;
      if (status != xmq_portable::PureDecodeStatus::InvalidMeterId &&
          (this->xmq_shadow_failed_ <= 3 ||
           this->xmq_shadow_failed_ % 30 == 0))
        ESP_LOGW(TAG, "XMQ direct decode rejected meter=%s driver=%s status=%u",
                 id.c_str(), driver.c_str(), static_cast<unsigned>(status));
      return;  // Strict mode: never decode/publish via legacy on failure.
    }
    for (size_t i = 0; i < decoded.value_count; ++i) {
      if (decoded.values[i].name && std::isfinite(decoded.values[i].value))
        this->xmq_values_.emplace_back(decoded.values[i].name,
                                       decoded.values[i].value);
    }
    if (this->xmq_values_.empty()) {
      ++this->xmq_shadow_failed_;
      return;
    }
    ++this->xmq_shadow_success_;
    if (this->xmq_shadow_success_ == 1 ||
        this->xmq_shadow_success_ % 30 == 0)
      ESP_LOGI(TAG, "XMQ direct publish meter=%s driver=%s ok=%u fail=%u fields=%u",
               id.c_str(), driver.c_str(),
               static_cast<unsigned>(this->xmq_shadow_success_),
               static_cast<unsigned>(this->xmq_shadow_failed_),
               static_cast<unsigned>(this->xmq_values_.size()));
    this->last_telegram = std::make_unique<Telegram>();
    this->last_telegram->about = AboutTelegram(
        App.get_friendly_name(), frame->rssi(), FrameType::WMBUS);
    this->defer([this]() {
      this->on_telegram_callback_manager();
      this->last_telegram = nullptr;
    });
    frame->mark_as_handled();
    return;
  }
#endif

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

#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
  // Preserve the independent Mode-5 numeric result for comparison only,
  // after legacy handleTelegram has processed this same frame.
  xmq_portable::CalcValue mode5_parity_values[24]{};
  size_t mode5_parity_count = 0;
  bool mode5_parity_ready = false;
#endif

  // Independent configured-meter XMQ diagnostic. Only the known plaintext
  // manufacturer CIs are passed through this path. CI=0x72/0x7A and other
  // encrypted or unsupported envelopes remain with legacy processing.
  // This is observation-only: no sensor publishing or handling state changes.
#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
  xmq_portable::CalcValue prelegacy_values[24]{};
  xmq_portable::DecodedPayload prelegacy_decoded{prelegacy_values, 24, 0, nullptr, 0, 0};
  bool prelegacy_ready = false;
  if (this->xmq_shadow_enabled_ &&
      header_result == wmbus_pure::HeaderStatus::Ok &&
      (pure_header.ci == 0xA0 || pure_header.ci == 0xA2)) {
    using namespace xmq_portable;
    Capture captures[12]{};
    CalendarDate dates[2]{};
    prelegacy_decoded.dates = dates;
    prelegacy_decoded.date_capacity = 2;
    const std::string configured_id = this->get_id();
    const std::string configured_driver = this->get_driver();
    const auto status = decode_configured_plain_frame(
        raw.data(), raw.size(), configured_id.c_str(),
        configured_driver.c_str(), captures, 12, prelegacy_decoded);
    prelegacy_ready = (status == PureDecodeStatus::Ok);
    if (status != PureDecodeStatus::InvalidMeterId) {
      static uint32_t checked = 0;
      static uint32_t passed = 0;
      static uint32_t failed = 0;
      ++checked;
      if (status == PureDecodeStatus::Ok) ++passed;
      else ++failed;
      if (checked == 1 || checked % 30 == 0 ||
          (status != PureDecodeStatus::Ok && failed <= 3))
        ESP_LOGI(TAG,
                 "Independent XMQ plain diagnostic checked=%u ok=%u fail=%u "
                 "status=%u driver=%s ci=0x%02X values=%u",
                 static_cast<unsigned>(checked),
                 static_cast<unsigned>(passed),
                 static_cast<unsigned>(failed),
                 static_cast<unsigned>(status), configured_driver.c_str(),
                 static_cast<unsigned>(pure_header.ci),
                 static_cast<unsigned>(prelegacy_decoded.value_count));
    }
  }
  // Mode-5 diagnostic: validate the configured ID, prepare spans/IV and
  // optionally decrypt in shadow, without publishing any values.
  // This is intentionally separate from the plain manufacturer decoder.
  if (this->xmq_shadow_enabled_ &&
      header_result == wmbus_pure::HeaderStatus::Ok &&
      (pure_header.ci == 0x72 || pure_header.ci == 0x7A)) {
    const std::string configured_id = this->get_id();
    if (wmbus_pure::match_meter_id(pure_header, configured_id.c_str()) ==
        wmbus_pure::MatchStatus::Match) {
      wmbus_pure::Mode5Prepared prepared{};
      const auto status = wmbus_pure::prepare_configured_mode5(
          raw.data(), raw.size(), pure_header.meter_id, prepared);
      static uint32_t mode5_checked = 0;
      static uint32_t mode5_ready = 0;
      ++mode5_checked;
      if (status == wmbus_pure::ConfiguredMode5Status::Ok) ++mode5_ready;
      // Never log a confidentiality key or its decrypted plaintext.
      auto *meter_keys = this->meter->meterKeys();
      const bool key_configured = meter_keys->hasConfidentialityKey();
      const std::string configured_driver = this->get_driver();
      // Diagnose readiness conservatively: a prepared Mode-5 telegram is
      // not proof its application payload can be decoded by the configured
      // XMQ driver. Report both gates independently.
      const bool xmq_driver_supported =
          configured_driver == "fhkvdataiii" ||
          configured_driver == "mkradio4";
      const bool mode5_candidate =
          status == wmbus_pure::ConfiguredMode5Status::Ok &&
          key_configured && xmq_driver_supported;
      // Shadow decryption reuses the configured key and existing mbedTLS
      // backend. Never log plaintext/key, modify legacy state or publish.
      // Only a 16-byte key may be passed to the AES-128 block adapter.
      if (mode5_candidate && meter_keys->confidentiality_key.size() == 16) {
        std::vector<uint8_t> plaintext;
        const auto decrypt_status = wmbus_pure::decrypt_configured_mode5(
            raw.data(), raw.size(), pure_header.meter_id,
            meter_keys->confidentiality_key.data(),
            wmbus_pure::mbedtls_aes128_decrypt_block, nullptr, plaintext);
        // Observe decoder compatibility only after successful decryption.
        // Mode-5 CI=72/7A is not a manufacturer A0/A2 envelope: even a
        // successful XMQ decode here remains diagnostic, never authoritative.
        if (decrypt_status == wmbus_pure::ConfiguredMode5DecryptStatus::Ok) {
          const auto *descriptor = xmq_portable::find_decode_descriptor(
              configured_driver.c_str(),
              configured_driver == "mkradio4" ? 0xA2 : 0xA0);
          if (descriptor) {
            xmq_portable::Capture captures[12]{};
            xmq_portable::CalcValue values[24]{};
            xmq_portable::CalendarDate dates[2]{};
            xmq_portable::DecodedPayload decoded{values, 24, 0, dates, 2, 0};
            const auto decode_status = xmq_portable::decode_mode5_plain_payload(
                *descriptor, plaintext.data(), plaintext.size(),
                captures, 12, decoded);
            static uint32_t xmq_checked = 0;
            static uint32_t xmq_ok = 0;
            ++xmq_checked;
            // A parser returning Ok with no recognized values is not a
            // usable decode. Keep this separate from crypto success.
            const bool has_fields =
                decode_status == xmq_portable::DriverStatus::Ok &&
                decoded.value_count > 0;
            // Diagnostic confidence only: require a minimal set of known
            // fields from the selected driver before counting a usable frame.
            // This is not a security/authenticity or publication decision.
            bool has_current = false;
            bool has_previous = false;
            bool has_total = false;
            for (size_t i = 0; i < decoded.value_count; ++i) {
              const char *name = decoded.values[i].name;
              if (!name || !std::isfinite(decoded.values[i].value)) continue;
              if (std::strcmp(name, "current_hca") == 0) has_current = true;
              if (std::strcmp(name, "previous_hca") == 0) has_previous = true;
              if (std::strcmp(name, "total") == 0) has_total = true;
            }
            const bool expected_fields =
                configured_driver == "fhkvdataiii"
                    ? (has_current && has_previous)
                    : (configured_driver == "mkradio4" && has_total);
            const bool usable = has_fields && expected_fields;
            if (usable) ++xmq_ok;
            if (usable && decoded.value_count <= 24) {
              for (size_t j = 0; j < decoded.value_count; ++j)
                mode5_parity_values[j] = decoded.values[j];
              mode5_parity_count = decoded.value_count;
              mode5_parity_ready = true;
            }

            if (xmq_checked == 1 || xmq_checked % 30 == 0 ||
                (!usable && xmq_checked <= 3))
              ESP_LOGI(TAG, "Independent Mode5 XMQ shadow checked=%u ok=%u "
                       "status=%u driver=%s values=%u has_fields=%u expected_fields=%u",
                       static_cast<unsigned>(xmq_checked),
                       static_cast<unsigned>(xmq_ok),
                       static_cast<unsigned>(decode_status),
                       configured_driver.c_str(),
                       static_cast<unsigned>(decoded.value_count),
                       static_cast<unsigned>(has_fields),
                       static_cast<unsigned>(expected_fields));
          }
        }
        static uint32_t decrypt_checked = 0;
        static uint32_t decrypt_ok = 0;
        ++decrypt_checked;
        if (decrypt_status == wmbus_pure::ConfiguredMode5DecryptStatus::Ok)
          ++decrypt_ok;
        if (decrypt_checked == 1 || decrypt_checked % 30 == 0 ||
            (decrypt_status != wmbus_pure::ConfiguredMode5DecryptStatus::Ok &&
             decrypt_checked <= 3))
          ESP_LOGI(TAG, "Independent Mode5 shadow decrypt checked=%u ok=%u "
                   "status=%u driver=%s",
                   static_cast<unsigned>(decrypt_checked),
                   static_cast<unsigned>(decrypt_ok),
                   static_cast<unsigned>(decrypt_status),
                   configured_driver.c_str());
      }
      if (mode5_checked == 1 || mode5_checked % 30 == 0 ||
          (status != wmbus_pure::ConfiguredMode5Status::Ok &&
           mode5_checked <= 3))
        ESP_LOGI(TAG,
                 "Independent Mode5 prepare checked=%u ready=%u status=%u "
                 "meter=%s driver=%s ci=0x%02X encrypted_bytes=%u "
                 "key_configured=%u xmq_driver_supported=%u candidate=%u",
                 static_cast<unsigned>(mode5_checked),
                 static_cast<unsigned>(mode5_ready),
                 static_cast<unsigned>(status), configured_id.c_str(),
                 configured_driver.c_str(),
                 static_cast<unsigned>(pure_header.ci),
                 static_cast<unsigned>(prepared.encrypted_size),
                 static_cast<unsigned>(key_configured),
                 static_cast<unsigned>(xmq_driver_supported),
                 static_cast<unsigned>(mode5_candidate));
    }
  }
  // Dates belong to the local decode block; numeric values are retained only
  // for the immediate comparison below, never for publication.
  prelegacy_decoded.dates = nullptr;
  prelegacy_decoded.date_capacity = 0;
#endif

  auto about =
      AboutTelegram(App.get_friendly_name(), frame->rssi(), FrameType::WMBUS);

  std::vector<Address> adresses;
  bool id_match = false;
  auto telegram = std::make_unique<Telegram>();

#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
  // Snapshot comparable legacy fields before this telegram is processed.
  // An unchanged value is inconclusive; only a changed value proves that
  // observable legacy state changed during this call.
  std::vector<double> mode5_legacy_before;
  if (mode5_parity_ready) {
    size_t snapshot_count = 0;
    const auto *snapshot_fields =
        xmq_portable::parity_fields(this->get_driver().c_str(), snapshot_count);
    for (size_t i = 0; snapshot_fields && i < snapshot_count; ++i) {
      std::string legacy_name;
      ::Unit legacy_unit;
      extractUnit(snapshot_fields[i].legacy_field, &legacy_name, &legacy_unit);
      mode5_legacy_before.push_back(
          this->meter->getNumericValue(legacy_name, legacy_unit));
    }
  }
#endif

#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
  // Plain A0/A2 parity: snapshot the legacy fields before processing.
  // Unchanged values remain inconclusive, not proof of stale data.
  std::vector<double> plain_legacy_before;
  if (prelegacy_ready) {
    size_t n = 0;
    const auto *p = xmq_portable::parity_fields(this->get_driver().c_str(), n);
    for (size_t k = 0; p && k < n; ++k) {
      std::string field_name;
      ::Unit unit;
      extractUnit(p[k].legacy_field, &field_name, &unit);
      plain_legacy_before.push_back(this->meter->getNumericValue(field_name, unit));
    }
  }
#endif

  this->meter->handleTelegram(about, frame->data(), false, &adresses, &id_match,
                              telegram.get());

#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
  if (mode5_parity_ready && id_match && !telegram->decryption_failed) {
    using namespace xmq_portable;
    DecodedPayload parity_decoded{mode5_parity_values, 24,
                                  mode5_parity_count, nullptr, 0, 0};
    const std::string name = this->get_driver();
    size_t count = 0;
    const ParityField *fields = parity_fields(name.c_str(), count);
    // No configured parity fields means no comparison was performed;
    // do not count this as a frame with matching results.
    if (!fields || count == 0) {
      ESP_LOGD(TAG, "Independent Mode5 parity unavailable driver=%s",
               name.c_str());
    } else {
    uint32_t equal = 0, different = 0, missing_xmq = 0, missing_legacy = 0;
    uint32_t legacy_changed = 0;
    uint32_t legacy_first_seen = 0;
    uint32_t legacy_unchanged = 0;
    uint32_t legacy_unavailable = 0;
    for (size_t i = 0; fields && i < count; ++i) {
      std::string legacy_name;
      ::Unit legacy_unit;
      extractUnit(fields[i].legacy_field, &legacy_name, &legacy_unit);
      const double legacy_value =
          this->meter->getNumericValue(legacy_name, legacy_unit);
      if (i < mode5_legacy_before.size()) {
        const double previous = mode5_legacy_before[i];
        if (!std::isfinite(legacy_value))
          ++legacy_unavailable;
        else if (!std::isfinite(previous))
          ++legacy_first_seen;
        else if (previous != legacy_value)
          ++legacy_changed;
        else
          ++legacy_unchanged;
      }
      double independent_value = NAN;
      switch (compare_numeric_field(parity_decoded, fields[i], legacy_value,
                                    std::isfinite(legacy_value), independent_value)) {
        case FieldParity::Equal: ++equal; break;
        case FieldParity::Different: ++different; break;
        case FieldParity::MissingXmq: ++missing_xmq; break;
        case FieldParity::MissingLegacy: ++missing_legacy; break;
      }
    }
    // A comparison is meaningful only if at least one field is present
    // on both sides. Missing fields must never count as a clean match.
    const uint32_t comparable = equal + different;
    // Without a per-frame legacy update marker, equality is an
    // observation, not proven fresh parity. Never count it as verified.
    const bool observed_match =
        comparable > 0 && different == 0 &&
        missing_xmq == 0 && missing_legacy == 0;
    const bool legacy_state_changed =
        legacy_changed > 0 || legacy_first_seen > 0;
    static uint32_t frames = 0;
    static uint32_t observed_matches = 0;
    static uint32_t incomparable = 0;
    static uint32_t mismatched = 0;
    ++frames;
    if (observed_match) ++observed_matches;
    if (comparable == 0) ++incomparable;
    if (!observed_match && comparable > 0) ++mismatched;
    if (frames == 1 || frames % 30 == 0 ||
        (!observed_match && frames <= 3))
      ESP_LOGI(TAG, "Independent Mode5 XMQ vs legacy parity driver=%s "
               "frames=%u comparable=%u equal=%u different=%u "
               "missing_xmq=%u missing_legacy=%u "
               "observed_match=%u observed_matches=%u incomparable=%u "
               "mismatched=%u legacy_changed=%u legacy_first_seen=%u "
               "legacy_unchanged=%u "
               "legacy_unavailable=%u legacy_state_changed=%u",
               name.c_str(), static_cast<unsigned>(frames),
               static_cast<unsigned>(comparable), static_cast<unsigned>(equal),
               static_cast<unsigned>(different),
               static_cast<unsigned>(missing_xmq),
               static_cast<unsigned>(missing_legacy),
               static_cast<unsigned>(observed_match),
               static_cast<unsigned>(observed_matches),
               static_cast<unsigned>(incomparable),
               static_cast<unsigned>(mismatched),
               static_cast<unsigned>(legacy_changed),
               static_cast<unsigned>(legacy_first_seen),
               static_cast<unsigned>(legacy_unchanged),
               static_cast<unsigned>(legacy_unavailable),
               static_cast<unsigned>(legacy_state_changed));
    }
  }
  // Numeric parity uses the real legacy state only after handleTelegram().
  // This observation cannot change sensors, handling flags or XMQ values.
  if (prelegacy_ready && id_match && !telegram->decryption_failed) {
    using namespace xmq_portable;
    const std::string name = this->get_driver();
    size_t count = 0;
    const ParityField *fields = parity_fields(name.c_str(), count);
    // A decoder may report Ok but provide no comparable fields.
    // This must never be counted as a successful parity frame.
    if (!fields || count == 0) {
      ESP_LOGD(TAG, "Independent XMQ plain parity unavailable driver=%s",
               name.c_str());
    } else {
    uint32_t equal = 0, different = 0, missing_xmq = 0, missing_legacy = 0;
    uint32_t legacy_changed = 0, legacy_first_seen = 0, legacy_unchanged = 0;
    for (size_t i = 0; fields && i < count; ++i) {
      std::string legacy_name;
      ::Unit legacy_unit;
      extractUnit(fields[i].legacy_field, &legacy_name, &legacy_unit);
      const double legacy_value = this->meter->getNumericValue(legacy_name, legacy_unit);
      if (i < plain_legacy_before.size() && std::isfinite(legacy_value)) {
        const double previous = plain_legacy_before[i];
        if (!std::isfinite(previous)) ++legacy_first_seen;
        else if (previous != legacy_value) ++legacy_changed;
        else ++legacy_unchanged;
      }
      double independent_value = NAN;
      const auto result = compare_numeric_field(
          prelegacy_decoded, fields[i], legacy_value,
          std::isfinite(legacy_value), independent_value);
      switch (result) {
        case FieldParity::Equal: ++equal; break;
        case FieldParity::Different: ++different; break;
        case FieldParity::MissingXmq: ++missing_xmq; break;
        case FieldParity::MissingLegacy: ++missing_legacy; break;
      }
      // Identify the failing field without dumping raw telegrams or keys.
      // Bounded logging avoids flooding the ESP32 when reception is noisy.
      if (result != FieldParity::Equal) {
        static uint32_t field_failures = 0;
        ++field_failures;
        if (field_failures <= 8 || field_failures % 100 == 0)
          ESP_LOGI(TAG, "Independent XMQ plain parity field driver=%s "
                   "field=%s status=%u failures=%u",
                   name.c_str(), fields[i].xmq_name,
                   static_cast<unsigned>(result),
                   static_cast<unsigned>(field_failures));
      }
    }
    const uint32_t comparable = equal + different;
    const bool observed_match =
        comparable > 0 && different == 0 &&
        missing_xmq == 0 && missing_legacy == 0;
    static uint32_t frames = 0, mismatched = 0, incomparable = 0;
    ++frames;
    if (!observed_match && comparable > 0) ++mismatched;
    if (comparable == 0) ++incomparable;
    if (frames == 1 || frames % 30 == 0 ||
        (different + missing_xmq + missing_legacy && mismatched <= 3))
      ESP_LOGI(TAG, "Independent XMQ vs legacy parity driver=%s frames=%u "
               "fields=%u equal=%u different=%u missing_xmq=%u "
               "missing_legacy=%u comparable=%u observed_match=%u "
               "mismatched_frames=%u incomparable=%u "
               "legacy_changed=%u legacy_first_seen=%u legacy_unchanged=%u",
               name.c_str(), static_cast<unsigned>(frames),
               static_cast<unsigned>(count), static_cast<unsigned>(equal),
               static_cast<unsigned>(different), static_cast<unsigned>(missing_xmq),
               static_cast<unsigned>(missing_legacy),
               static_cast<unsigned>(comparable),
               static_cast<unsigned>(observed_match),
               static_cast<unsigned>(mismatched),
               static_cast<unsigned>(incomparable),
               static_cast<unsigned>(legacy_changed),
               static_cast<unsigned>(legacy_first_seen),
               static_cast<unsigned>(legacy_unchanged));
    }
  }
#endif

  if (id_match && this->xmq_shadow_enabled_) {
    // Never reuse values from a previous telegram in authoritative mode.
    if (this->xmq_only_enabled_) this->xmq_values_.clear();
#if defined(WMBUS_XMQ_TABLES_AVAILABLE)
    // Opt-in strict authority: only independently decoded plaintext A0/A2
    // results may be published. A failed/missing pre-legacy decode leaves
    // the frame unhandled: never fall back to stale or post-legacy readings.
    // Legacy still runs above for matching and callback lifecycle; this flag
    // deliberately does NOT claim an end-to-end legacy-free implementation.
    if (this->xmq_pure_publish_enabled_ && prelegacy_ready &&
        (pure_header.ci == 0xA0 || pure_header.ci == 0xA2)) {
      for (size_t i = 0; i < prelegacy_decoded.value_count; ++i) {
        const auto &v = prelegacy_decoded.values[i];
        if (v.name != nullptr && std::isfinite(v.value))
          this->xmq_values_.emplace_back(v.name, v.value);
      }
    }

    // Bounded diagnostic for fhkvdataiii: expose which legacy-dependent gate
    // prevents field parity, without changing decoder or publication behavior.
    const bool xmq_hca_diag = this->meter->driverName().str() == "fhkvdataiii";
    static uint32_t xmq_hca_entered = 0;
    static uint32_t xmq_hca_rejected = 0;
    if (xmq_hca_diag) {
      ++xmq_hca_entered;
      if (telegram->decryption_failed || telegram->header_size < 0 ||
          telegram->suffix_size < 0 || telegram->frame.empty()) {
        ++xmq_hca_rejected;
        if (xmq_hca_rejected <= 3 || xmq_hca_rejected % 30 == 0)
          ESP_LOGW(TAG, "XMQ HCA parity gate meter=%s entered=%u rejected=%u decrypt_failed=%u header=%d suffix=%d frame_len=%u",
                   this->get_id().c_str(), static_cast<unsigned>(xmq_hca_entered),
                   static_cast<unsigned>(xmq_hca_rejected),
                   static_cast<unsigned>(telegram->decryption_failed),
                   telegram->header_size, telegram->suffix_size,
                   static_cast<unsigned>(telegram->frame.size()));
      }
    }
    // Observation only: do not call mark_as_handled(), change meter state,
    // or publish any values. Run only after legacy ID matching and decryption.
    if (!telegram->decryption_failed &&
        telegram->header_size >= 0 && telegram->suffix_size >= 0 &&
        !telegram->frame.empty()) {
      const auto name = this->meter->driverName().str();
      const auto *descriptor = xmq_portable::find_decode_descriptor(
          name.c_str(), pure_header.ci);
      if (!descriptor && xmq_hca_diag) {
        static uint32_t xmq_hca_no_descriptor = 0;
        ++xmq_hca_no_descriptor;
        if (xmq_hca_no_descriptor <= 3 || xmq_hca_no_descriptor % 30 == 0)
          ESP_LOGW(TAG, "XMQ HCA parity descriptor missing meter=%s ci=0x%02X count=%u",
                   this->get_id().c_str(), static_cast<unsigned>(pure_header.ci),
                   static_cast<unsigned>(xmq_hca_no_descriptor));
      }
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
        if (xmq_hca_diag) {
          static uint32_t xmq_hca_decodes = 0;
          static uint32_t xmq_hca_failed = 0;
          ++xmq_hca_decodes;
          if (status != DriverStatus::Ok) ++xmq_hca_failed;
          if (xmq_hca_decodes == 1 || xmq_hca_decodes % 30 == 0 ||
              (status != DriverStatus::Ok && xmq_hca_failed <= 3))
            ESP_LOGI(TAG, "XMQ HCA secondary decode meter=%s attempted=%u failed=%u status=%u values=%u xmq_only=%u",
                     this->get_id().c_str(), static_cast<unsigned>(xmq_hca_decodes),
                     static_cast<unsigned>(xmq_hca_failed), static_cast<unsigned>(status),
                     static_cast<unsigned>(decoded.value_count),
                     static_cast<unsigned>(this->xmq_only_enabled_));
        }
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
        // Compare decoded XMQ with the legacy decoder even in xmq_only mode.
        // handleTelegram() updates legacy state; parity does not change publishing.
        if (status == DriverStatus::Ok) {
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
          // Global, per-driver numeric parity summary. Unlike the per-meter
          // 1/31/61 diagnostic this aggregates all configured meters and thus
          // remains visible in short serial-log captures. Diagnostic only.
          struct ParityTotals {
            uint32_t frames{0};
            uint32_t equal{0};
            uint32_t different{0};
            uint32_t missing_xmq{0};
            uint32_t missing_legacy{0};
          };
          static ParityTotals hca_totals{};
          static ParityTotals water_totals{};
          ParityTotals *totals = nullptr;
          if (name == "fhkvdataiii") totals = &hca_totals;
          else if (name == "mkradio4") totals = &water_totals;
          if (totals != nullptr) {
            ++totals->frames;
            totals->equal += equal;
            totals->different += different;
            totals->missing_xmq += missing_xmq;
            totals->missing_legacy += missing_legacy;
            if (totals->frames == 1 || totals->frames % 30 == 0 ||
                ((different || missing_xmq || missing_legacy) && totals->frames <= 4)) {
              ESP_LOGI(TAG, "XMQ field parity summary driver=%s frames=%u equal=%u different=%u missing_xmq=%u missing_legacy=%u",
                       name.c_str(), static_cast<unsigned>(totals->frames),
                       static_cast<unsigned>(totals->equal),
                       static_cast<unsigned>(totals->different),
                       static_cast<unsigned>(totals->missing_xmq),
                       static_cast<unsigned>(totals->missing_legacy));
            }
          }
          if (this->xmq_field_parity_checks_ % 30 == 1 ||
              (different + missing_xmq + missing_legacy && this->xmq_field_parity_checks_ <= 4)) {
            ESP_LOGI(TAG, "XMQ field parity meter=%s xmq_only=%u frames=%u equal=%u different=%u missing_xmq=%u missing_legacy=%u",
                     this->get_id().c_str(),
                     static_cast<unsigned>(this->xmq_only_enabled_),
                     static_cast<unsigned>(this->xmq_field_parity_checks_),
                     static_cast<unsigned>(this->xmq_field_parity_equal_),
                     static_cast<unsigned>(this->xmq_field_parity_different_),
                     static_cast<unsigned>(this->xmq_field_parity_missing_xmq_),
                     static_cast<unsigned>(this->xmq_field_parity_missing_legacy_));
          }
        }
        if (status == DriverStatus::Ok) {
          if (this->xmq_only_enabled_ && !this->xmq_pure_publish_enabled_) {
            this->xmq_values_.clear();
            for (size_t i = 0; i < decoded.value_count; ++i)
              this->xmq_values_.emplace_back(decoded.values[i].name, decoded.values[i].value);
          }
          ++this->xmq_shadow_success_;
        } else {
          if (this->xmq_only_enabled_ && !this->xmq_pure_publish_enabled_)
            this->xmq_values_.clear();
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
