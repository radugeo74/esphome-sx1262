#include <cmath>
#include <cstdint>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "record_decoder.h"
#include "generated_ixml.h"
#include "numeric_bridge.h"
#include "driver_runtime.h"
#include "apator162_ixml_fields.h"
#include "kamwater_status_semantics.h"
#include "kamwater_numeric_semantics.h"
#include "kamwater_tpl_status_semantics.h"
#include "minomess_wired_frame.h"
#include "oms_ccm_mbedtls_probe.h"
#include "kamwater_ccm_original_fixtures.h"
#include "frame_bridge.h"
#include <cstring>

static const char *TAG = "xmq_cpp_probe";

extern "C" void app_main(void) {
  using namespace xmq_portable;
  const uint8_t raw[2] = {0xBD, 0x08};
  Record record{};
  const Status result = decode_record("0265", 4, raw, 2, record);
  if (result != Status::Ok || record.unit != Unit::TemperatureC ||
      std::fabs(record.scaled - 22.37) > 0.0001) {
    ESP_LOGE(TAG, "Portable C++ record decoder self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "Portable C++ DIF/VIF self-test PASS: %.2f C", record.scaled);

  // Real IXML rule tables generated from unmodified upstream XMQ definitions.
  const uint8_t water[] = {0x06, 0x70, 0x29, 0x01, 0x00, 0x60, 0x17,
                           0x03, 0x00, 0x04, 0x00, 0x03, 0x00, 0x00, 0x00};
  Capture captured[8]{};
  Result parsed{captured, 8, 0, 0};
  const ParseStatus parsed_status = execute(
      generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
      "decode", water, sizeof(water), parsed);
  if (parsed_status != ParseStatus::Ok || parsed.count != 2 ||
      parsed.consumed != sizeof(water) ||
      std::strcmp(parsed.captures[0].name, "DV_prev") != 0 ||
      parsed.captures[0].length != 2 ||
      parsed.captures[0].data[0] != 1 ||
      std::strcmp(parsed.captures[1].name, "DV_curr") != 0 ||
      parsed.captures[1].length != 2 ||
      parsed.captures[1].data[0] != 3) {
    ESP_LOGE(TAG, "Generated IXML C++ self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "Generated mkradio4 IXML self-test PASS, captures=%u",
           static_cast<unsigned>(parsed.count));
  NumericValue numeric{};
  if (decode_mapped_water(parsed, generated_ixml::mkradio4::capture_keys, 2, generated_ixml::mkradio4::field_keys, 2, numeric) != NumericStatus::Ok ||
      std::fabs(numeric.target_m3 - 0.1) > 0.00001 ||
      std::fabs(numeric.total_m3 - 0.4) > 0.00001) {
    ESP_LOGE(TAG, "C++ IXML-to-numeric self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "C++ water values: target=%.3f m3 total=%.3f m3",
           numeric.target_m3, numeric.total_m3);
  CalcValue calculated[16]{};
  size_t calculated_count = 0;
  if (seed_calculation_fields(parsed, generated_ixml::mkradio4::capture_keys, 2,
                              generated_ixml::mkradio4::field_keys, 2,
                              calculated, calculated_count, 16) != NumericStatus::Ok ||
      calculate(generated_ixml::mkradio4::calculations,
                generated_ixml::mkradio4::calculation_count,
                calculated, calculated_count, 16) != CalcStatus::Ok) {
    ESP_LOGE(TAG, "Generated XMQ calculate self-test failed");
    abort();
  }
  bool target_ok = false, total_ok = false;
  for (size_t i = 0; i < calculated_count; ++i) {
    if (std::strcmp(calculated[i].name, "target") == 0)
      target_ok = std::fabs(calculated[i].value - 0.1) < 0.00001;
    if (std::strcmp(calculated[i].name, "total") == 0)
      total_ok = std::fabs(calculated[i].value - 0.4) < 0.00001;
  }
  if (!target_ok || !total_ok) {
    ESP_LOGE(TAG, "Unexpected generated XMQ calculate values");
    abort();
  }
  ESP_LOGI(TAG, "Generated XMQ calculate fields PASS");
  // Calendar metadata comes from the pinned fhkvdataiii XMQ PointInTime rules.
  CalcValue heat_dates[16] = {{"previous_date_raw", 0x299F},
                              {"current_date_raw", 0x2450}};
  size_t heat_date_count = 2;
  if (calculate(generated_ixml::fhkvdataiii::calculations,
                generated_ixml::fhkvdataiii::calculation_count,
                heat_dates, heat_date_count, 16) != CalcStatus::Ok) {
    ESP_LOGE(TAG, "Generated heat date components failed");
    abort();
  }
  CalendarDate calendar_dates[2]{};
  if (calculate_dates(generated_ixml::fhkvdataiii::date_calculations,
                      generated_ixml::fhkvdataiii::date_calculation_count,
                      heat_dates, heat_date_count, calendar_dates, 2) != CalendarStatus::Ok ||
      calendar_dates[0].year != 2021 || calendar_dates[0].month != 2 ||
      calendar_dates[0].day != 5 || calendar_dates[1].year != 2020 ||
      calendar_dates[1].month != 12 || calendar_dates[1].day != 31) {
    ESP_LOGE(TAG, "Generated calendar fields failed");
    abort();
  }
  ESP_LOGI(TAG, "Generated XMQ Gregorian calendar self-test PASS");
  // Exercise the same bounded driver runtime used by the host tests.
  const DriverTables water_tables{
    generated_ixml::mkradio4::rules, generated_ixml::mkradio4::count,
    generated_ixml::mkradio4::capture_keys, 2,
    generated_ixml::mkradio4::field_keys, 2,
    generated_ixml::mkradio4::calculations, generated_ixml::mkradio4::calculation_count,
    nullptr, 0};
  Capture runtime_captures[12]{};
  CalcValue runtime_values[20]{};
  CalendarDate runtime_dates[2]{};
  DecodedPayload runtime_output{runtime_values, 20, 0, runtime_dates, 2, 0};
  if (decode_driver_payload(water_tables, water, sizeof(water),
                            runtime_captures, 12, runtime_output) != DriverStatus::Ok) {
    ESP_LOGE(TAG, "Generic generated driver runtime self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "Generated driver runtime self-test PASS");
  PayloadSpan borrowed{};
  if (application_payload(water, sizeof(water), 0, 0, borrowed) != PayloadStatus::Ok ||
      decode_parsed_frame(water_tables, water, sizeof(water), 0, 0,
                          runtime_captures, 12, runtime_output) != DriverStatus::Ok ||
      runtime_output.value_count == 0) {
    ESP_LOGE(TAG, "Parsed frame bridge self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "Parsed frame bridge self-test PASS");
  const DriverTables heat_tables{
    generated_ixml::fhkvdataiii::rules, generated_ixml::fhkvdataiii::count,
    generated_ixml::fhkvdataiii::capture_keys, 6,
    generated_ixml::fhkvdataiii::field_keys, 2,
    generated_ixml::fhkvdataiii::calculations,
    generated_ixml::fhkvdataiii::calculation_count,
    generated_ixml::fhkvdataiii::date_calculations,
    generated_ixml::fhkvdataiii::date_calculation_count};
  const uint8_t heat_application[] = {
    0x01,0x02, // preceding application records
    0x0F,0x9F,0x29,0x2D,0x00,0x50,0x24,0x04,0x00,
    0x11,0xBD,0x08,0x38,0x09,0x04,0x00,0x00,0x00,
    0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,
    0x00,0x00,0x03,0x14,0x0E};
  if (decode_manufacturer_frame(heat_tables, heat_application,
       sizeof(heat_application), 0, 0, 2, runtime_captures, 12,
       runtime_output) != DriverStatus::Ok ||
       runtime_output.date_count != 2 ||
       runtime_dates[0].year != 2021 || runtime_dates[0].month != 2 ||
       runtime_dates[0].day != 5) {
    ESP_LOGE(TAG, "Manufacturer payload XMQ bridge self-test failed");
    abort();
  }
  ESP_LOGI(TAG, "Manufacturer 0x0F payload XMQ bridge PASS");


  // Stage A: compile and exercise the EXACT Apator162 portable algorithm
  // used by source XMQ host fixture #19. This is already-clear application
  // input; no radio path or cryptographic authenticity is being asserted.
  const uint8_t apator162_fixture[] = {0x0F, 0x64, 0x8E, 0x7A, 0x9A, 0x0C, 0x02, 0x08, 0x43, 0x90, 0x00, 0x82, 0x62, 0x85, 0x57, 0xD2, 0x48, 0x57, 0x0C, 0xF1, 0x58, 0x00, 0x10, 0x58, 0x1D, 0x00, 0x00, 0x70, 0x01, 0x53, 0x1D, 0x00, 0x00, 0xA0, 0x9D, 0xE1, 0x9D, 0x02, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0C, 0x2D};
  xmq_portable::apator162::Reading apator162_reading{};
  if (xmq_portable::apator162::decode_clear_manufacturer(
          apator162_fixture, sizeof(apator162_fixture), apator162_reading)
          != xmq_portable::apator162::Status::Ok ||
      std::fabs(apator162_reading.total_m3 - 7.512) > 0.000001) {
    ESP_LOGE(TAG, "Shared Apator162 XMQ application regression failed");
    abort();
  }
  // Invalid manufacturer marker is rejected without a reading.
  const uint8_t truncated_apator[] = {0x0F, 0x00};
  if (xmq_portable::apator162::decode_clear_manufacturer(
          truncated_apator, sizeof(truncated_apator), apator162_reading)
          == xmq_portable::apator162::Status::Ok ||
      apator162_reading.total_m3 != 0.0) {
    ESP_LOGE(TAG, "Shared Apator162 fail-closed regression failed");
    abort();
  }
  ESP_LOGI(TAG, "Shared native Apator162 application regression PASS (not RF)");


  // Stage A Kamwater: the exact same native C++ pure-application logic is
  // exercised by the original XMQ host golden tests; no RF admission.
  const uint8_t kw_status_fixture[] = {
      0x02,0xFF,0x20,0x71,0x00,0x04,0x13,0x08,0x19,0x00,0x00,
      0x44,0x13,0x08,0x19,0x00,0x00,0x61,0x5B,0x7F,0x61,0x67,0x13};
  kamwater::status_semantics::Reading kw_status{};
  if (kamwater::status_semantics::decode(
          kw_status_fixture,sizeof(kw_status_fixture),kw_status)
          != kamwater::status_semantics::Status::Ok ||
      kw_status.status != "DRY" || kw_status.current_status != "DRY" ||
      kw_status.time_dry != "22-31 days") {
    ESP_LOGE(TAG, "Shared Kamwater source #1 status regression failed");
    abort();
  }

  // Original Kamwater #3 already-clear XMQ bytes include the separately
  // reviewed prefix 46 88 78. This sample checks only the DIF/VIF numeric
  // part; no generic RF parser or arbitrary prefix-stripping is enabled.
  const uint8_t kw_numeric_fixture[] = {
      0x02,0xFF,0x20,0x00,0x00,0x04,0x13,0x2F,0x4E,0x00,0x00,
      0x92,0x01,0x3B,0x3D,0x01,0xA1,0x01,0x5B,0x02,
      0x81,0x01,0xE7,0xFF,0x0F,0x03};
  kamwater::numeric_semantics::Reading kw_numeric{};
  if (kamwater::numeric_semantics::decode(
          kw_numeric_fixture,sizeof(kw_numeric_fixture),kw_numeric)
          != kamwater::numeric_semantics::Status::Ok) {
    ESP_LOGE(TAG, "Shared Kamwater source #3 numeric parse failed");
    abort();
  }
  bool kw_flow_ok=false, kw_temp_ok=false;
  for (const auto &field : kw_numeric.fields) {
    if (field.name=="max_flow_last_day_m3h")
      kw_flow_ok=std::fabs(field.value-0.317)<0.000001;
    if (field.name=="min_flow_temperature_last_day_c")
      kw_temp_ok=std::fabs(field.value-2.0)<0.000001;
  }
  if (!kw_flow_ok || !kw_temp_ok) {
    ESP_LOGE(TAG, "Shared Kamwater source #3 numeric values differ");
    abort();
  }
  const uint8_t truncated_kw_status[] = {0x02,0xFF,0x20,0x71};
  if (kamwater::status_semantics::decode(
          truncated_kw_status,sizeof(truncated_kw_status),kw_status)
          == kamwater::status_semantics::Status::Ok ||
      !kw_status.status.empty()) {
    ESP_LOGE(TAG, "Shared Kamwater fail-closed status regression failed");
    abort();
  }
  ESP_LOGI(TAG, "Shared Kamwater clear application C++ regression PASS (not RF)");


  // Stage A: real source Kamwater #14 already-clear TPL CI=7A application.
  const uint8_t kw_tpl_fixture[] = {0x7A,0x44,0x02,0x00,0x00,
                                     0x04,0xFF,0x23,0x01,0x00,0x00,0x00};
  kamwater::tpl_status::Reading kw_tpl{};
  if (kamwater::tpl_status::decode_clear_short_tpl(
          kw_tpl_fixture,sizeof(kw_tpl_fixture),kw_tpl)
          != kamwater::tpl_status::Status::Ok ||
      kw_tpl.status!="DRY ERROR" || !kw_tpl.has_durations ||
      kw_tpl.durations.status!="DRY" || !kw_tpl.durations.time_dry.empty()) {
    ESP_LOGE(TAG, "Shared Kamwater source #14 TPL status mismatch");
    abort();
  }
  const uint8_t truncated_tpl[]={0x7A,0x44,0x02,0x00,0x00};
  if (kamwater::tpl_status::decode_clear_short_tpl(
          truncated_tpl,sizeof(truncated_tpl),kw_tpl)
          == kamwater::tpl_status::Status::Ok ||
      !kw_tpl.status.empty()) {
    ESP_LOGE(TAG, "Kamwater truncated TPL unexpectedly accepted");
    abort();
  }

  // Original source Minomess wired #1: distinct 0x68 M-Bus envelope
  // with SUM checksum, not a wireless T1/C1 DLL CRC!
  const uint8_t wired_mino[] = {0x68,0x5F,0x5F,0x68,0x08,0x00,0x72,0x20,0x33,0x36,0x15,0x49,0x6A,0x01,0x16,0xA3,0x00,0x00,0x00,0x0C,0x13,0x97,0x75,0x05,0x00,0x02,0x6C,0x51,0x3A,0x82,0x04,0x6C,0x41,0x3A,0x8C,0x04,0x13,0xAA,0xDB,0x00,0x00,0x8D,0x04,0x93,0x13,0x2C,0x33,0xFE,0xD0,0xD2,0x00,0xA6,0xCD,0x00,0x7E,0xC8,0x00,0x9C,0xC1,0x00,0x48,0xBA,0x00,0xFA,0xB3,0x00,0x9B,0xAD,0x00,0xD2,0xA8,0x00,0x28,0xA2,0x00,0x4F,0x9B,0x00,0xF6,0x94,0x00,0x9B,0x8F,0x00,0xF9,0x89,0x00,0x48,0x86,0x00,0x02,0xFD,0x17,0x00,0x00,0x01,0xFD,0x71,0xAA,0xE7,0x16};
  minomess::wired::Reading wired_reading{};
  if (minomess::wired::decode(wired_mino,sizeof(wired_mino),wired_reading)
          != minomess::wired::Status::Ok ||
      std::fabs(wired_reading.meter.total_m3-57.597)>0.000001 ||
      !wired_reading.has_gateway_rssi ||
      wired_reading.gateway_rssi_dbm!=-86 ||
      wired_reading.meter.status_bits!=0) {
    ESP_LOGE(TAG, "Shared wired Minomess source #1 frame mismatch");
    abort();
  }
  // Corrupt the checksum, not the data: the decoder must reject.
  uint8_t bad_wired[sizeof(wired_mino)]{};
  std::memcpy(bad_wired,wired_mino,sizeof(wired_mino));
  bad_wired[sizeof(wired_mino)-2]^=1;
  if (minomess::wired::decode(bad_wired,sizeof(bad_wired),wired_reading)
          == minomess::wired::Status::Ok ||
      wired_reading.has_gateway_rssi ||
      wired_reading.meter.total_m3!=0.0) {
    ESP_LOGE(TAG, "Wired M-Bus checksum fail-closed regression failed");
    abort();
  }
  ESP_LOGI(TAG,"Shared Kamwater TPL + wired Minomess ESP-IDF checks PASS (not RF)");
  
  // Stage B: compile/link the ESP-IDF mbedTLS AES-CMAC + CCM code.
  // Dummy ciphertext has an invalid tag, so no plaintext may emerge.
  // CI builds this app_main; it does NOT run on hardware in Actions.
  const uint8_t ccm_dummy_m[2]={0x37,0x2C};
  const uint8_t ccm_dummy_id[4]={0x06,0x03,0x82,0x53};
  const uint8_t ccm_dummy_key[16]{};
  const uint8_t ccm_dummy_tpl[20]={
      0x7A,0x30,0x10,0xFF,0x2A,0x10,0x01,0x01,0,0,0,
      0,0,0,0,0,0,0,0,0};
  oms_ccm_idf_probe::Output ccm_probe_output{};
  if (oms_ccm_idf_probe::decode_short_tpl(
          ccm_dummy_m,ccm_dummy_id,0x3C,0x16,ccm_dummy_key,
          ccm_dummy_tpl,sizeof(ccm_dummy_tpl),ccm_probe_output)
          != oms_ccm_idf_probe::Status::BadTag ||
      !ccm_probe_output.clear_application.empty()) {
    ESP_LOGE(TAG,"ESP-IDF mbedTLS CCM fail-closed probe failed");
    abort();
  }
  ESP_LOGI(TAG, "Free heap: %lu", static_cast<unsigned long>(esp_get_free_heap_size()));
  ESP_LOGI(TAG, "Largest free block: %lu",
           static_cast<unsigned long>(heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));
}
