#include <cassert>
#include <cstring>
#include <cstdio>
#include <cmath>
#include "../../../components/wmbus_radio/pure_frame_header.h"
#include "../../../components/wmbus_radio/pure_meter_match.h"
#include "../../../components/wmbus_meter/xmq_decode_catalog.h"
int main() {
  using xmq_portable::find_decode_descriptor;
  const auto *heat = find_decode_descriptor("fhkvdataiii", 0xA0);
  const auto *water = find_decode_descriptor("mkradio4", 0xA2);
  assert(heat && water);
  assert(heat->payload_policy == xmq_portable::PayloadPolicy::PreferManufacturer0f);
  assert(water->payload_policy == xmq_portable::PayloadPolicy::Application);
  assert(heat->tables.rules && heat->tables.rule_count > 0);
  assert(water->tables.rules && water->tables.rule_count > 0);
  assert(heat->tables.date_calculation_count > 0);
  assert(water->tables.date_calculation_count == 0);
  assert(find_decode_descriptor("fhkvdataiii", 0xA2) == nullptr);
  assert(find_decode_descriptor("mkradio4", 0xA0) == nullptr);
  assert(find_decode_descriptor("unknown", 0xA0) == nullptr);
  assert(find_decode_descriptor(nullptr, 0xA0) == nullptr);
  const uint8_t frame[]{0xAA, 0x0F, 0x11, 0x22, 0xBB};
  xmq_portable::PayloadSpan payload{};
  assert(xmq_portable::descriptor_payload(*heat, frame, sizeof(frame), 1, 1, 0, payload)
         == xmq_portable::PayloadStatus::Ok);
  assert(payload.data == frame + 1 && payload.length == 3);
  assert(xmq_portable::descriptor_payload(*heat, frame, sizeof(frame), 1, 1, -1, payload)
         == xmq_portable::PayloadStatus::Ok);
  assert(payload.data == frame + 1 && payload.length == 3);
  assert(xmq_portable::descriptor_payload(*heat, frame, sizeof(frame), 1, 1, 2, payload)
         == xmq_portable::PayloadStatus::InvalidBounds);
  assert(xmq_portable::descriptor_payload(*water, frame, sizeof(frame), 1, 1, 2, payload)
         == xmq_portable::PayloadStatus::Ok);

  // Upstream mkradio4 XMQ fixture; no user capture or meter identifiers.
  const uint8_t official_water[] = {
    0x2f,0x44,0x68,0x50,0x20,0x01,0x41,0x02,0x95,0x62,0xa2,
    0x06,0x70,0x29,0x01,0x00,0x60,0x17,0x03,0x00,0x04,0x00,0x03,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00
  };
  wmbus_pure::Header parsed{};
  assert(wmbus_pure::parse_header(official_water, sizeof(official_water), parsed)
         == wmbus_pure::HeaderStatus::Ok);
  assert(wmbus_pure::match_meter_id(parsed, "02410120")
         == wmbus_pure::MatchStatus::Match);
  assert(wmbus_pure::match_meter_id(parsed, "02410121")
         == wmbus_pure::MatchStatus::DifferentId);
  const auto *selected = find_decode_descriptor("mkradio4", parsed.ci);
  assert(selected != nullptr);
  xmq_portable::Capture captures[12]{};
  xmq_portable::CalcValue numeric[24]{};
  xmq_portable::CalendarDate dates[2]{};
  xmq_portable::DecodedPayload decoded{numeric,24,0,dates,2,0};
  assert(xmq_portable::decode_driver_payload(selected->tables,
       parsed.application, parsed.application_length, captures,12,decoded)
       == xmq_portable::DriverStatus::Ok);
  bool found_target=false, found_total=false;
  for (size_t i=0;i<decoded.value_count;++i) {
    if (std::strcmp(decoded.values[i].name,"target")==0) {
      assert(std::fabs(decoded.values[i].value-0.1)<1e-6);
      found_target=true;
    }
    if (std::strcmp(decoded.values[i].name,"total")==0) {
      assert(std::fabs(decoded.values[i].value-0.4)<1e-6);
      found_total=true;
    }
  }
  assert(found_target && found_total);
  // A damaged header cannot be selected; an incomplete payload must not
  // retain values from a prior successful decode.
  uint8_t damaged[sizeof(official_water)]{};
  std::memcpy(damaged,official_water,sizeof(official_water));
  damaged[0]=0x01;
  assert(wmbus_pure::parse_header(damaged,sizeof(damaged),parsed)
         == wmbus_pure::HeaderStatus::InvalidLength);
  assert(xmq_portable::decode_driver_payload(selected->tables,
       official_water+11,2,captures,12,decoded)
       != xmq_portable::DriverStatus::Ok);
  assert(decoded.value_count==0 && decoded.date_count==0);

  // Upstream fhkvdataiii 0F fixture with a complete CRC-stripped DLL header.
  // The manufacturer-specific payload goes directly into the generated decoder;
  // no legacy Telegram, mfct_0f_index, or handleTelegram() is involved.
  const uint8_t heat_frame[] = {
    0x33,0x44,0x68,0x50,0x34,0x12,0x11,0x11,0x94,0x80,0xa2,
    0x0f,0x9f,0x29,0x2d,0x00,0x50,0x24,0x04,0x00,0x11,0xbd,
    0x08,0x38,0x09,0x04,0x00,0x00,0x00,0x07,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,
    0x00,0x00,0x00,0x00,0x00,0x03,0x14,0x0e
  };
  wmbus_pure::Header heat_header{};
  assert(wmbus_pure::parse_header(heat_frame, sizeof(heat_frame), heat_header)
         == wmbus_pure::HeaderStatus::Ok);
  assert(heat_header.ci==0xA2 && heat_header.meter_id==11111234u);
  assert(wmbus_pure::match_meter_id(heat_header, "11111234")
         == wmbus_pure::MatchStatus::Match);
  // The driver is selected by the configured meter type. The 0F payload
  // variant can arrive behind CI A2 on actual Techem fixtures.
  const auto *heat_a2 = find_decode_descriptor("fhkvdataiii", 0xA0);
  assert(heat_a2 != nullptr);
  assert(xmq_portable::decode_driver_payload(heat_a2->tables,
       heat_header.application, heat_header.application_length,
       captures,12,decoded)==xmq_portable::DriverStatus::Ok);
  auto expected_heat = [&](const char *name, double want) {
    for (size_t i=0;i<decoded.value_count;++i)
      if (decoded.values[i].name &&
          std::strcmp(decoded.values[i].name,name)==0)
        return std::fabs(decoded.values[i].value-want)<1e-6;
    return false;
  };
  assert(expected_heat("current_hca",4.0));
  assert(expected_heat("previous_hca",45.0));
  assert(expected_heat("temp_room_c",22.37));
  assert(expected_heat("temp_radiator_c",23.6));
  assert(decoded.date_count==2);
  assert(xmq_portable::decode_driver_payload(heat_a2->tables,
       heat_header.application, 2,
       captures,12,decoded)!=xmq_portable::DriverStatus::Ok);
  assert(decoded.value_count==0 && decoded.date_count==0);

  // The shared pipeline must accept the narrowly-defined 0F heat variant.
  assert(xmq_portable::decode_configured_plain_frame(
      heat_frame, sizeof(heat_frame), "11111234", "fhkvdataiii",
      captures, 12, decoded) == xmq_portable::PureDecodeStatus::Ok);
  assert(decoded.date_count == 2);
  bool pure_heat_current = false;
  for (size_t i=0; i<decoded.value_count; ++i)
    if (decoded.values[i].name &&
        std::strcmp(decoded.values[i].name, "current_hca") == 0) {
      assert(std::fabs(decoded.values[i].value - 4.0) < 1e-6);
      pure_heat_current = true;
    }
  assert(pure_heat_current);
  // A changed version or missing 0F marker must never pass this exception.
  uint8_t unsupported_heat[sizeof(heat_frame)]{};
  std::memcpy(unsupported_heat, heat_frame, sizeof(heat_frame));
  unsupported_heat[8] = 0x95;
  assert(xmq_portable::decode_configured_plain_frame(
      unsupported_heat, sizeof(unsupported_heat), "11111234", "fhkvdataiii",
      captures, 12, decoded) == xmq_portable::PureDecodeStatus::UnknownDriver);
  assert(decoded.value_count == 0 && decoded.date_count == 0);
  unsupported_heat[8] = 0x94;
  unsupported_heat[11] = 0x10;
  assert(xmq_portable::decode_configured_plain_frame(
      unsupported_heat, sizeof(unsupported_heat), "11111234", "fhkvdataiii",
      captures, 12, decoded) == xmq_portable::PureDecodeStatus::UnknownDriver);
  assert(decoded.value_count == 0 && decoded.date_count == 0);

  // Shared independent plain-frame entry point: no legacy Telegram.
  using xmq_portable::decode_configured_plain_frame;
  using xmq_portable::PureDecodeStatus;
  assert(decode_configured_plain_frame(official_water, sizeof(official_water),
      "02410120", "mkradio4", captures, 12, decoded) == PureDecodeStatus::Ok);
  assert(decoded.value_count > 0);
  assert(decode_configured_plain_frame(official_water, sizeof(official_water),
      "02410121", "mkradio4", captures, 12, decoded)
      == PureDecodeStatus::InvalidMeterId);
  assert(decoded.value_count == 0 && decoded.date_count == 0);
  assert(decode_configured_plain_frame(official_water, sizeof(official_water),
      "02410120", "fhkvdataiii", captures, 12, decoded)
      == PureDecodeStatus::UnknownDriver);
  assert(decoded.value_count == 0 && decoded.date_count == 0);
  assert(decode_configured_plain_frame(damaged, sizeof(damaged),
      "02410120", "mkradio4", captures, 12, decoded)
      == PureDecodeStatus::InvalidFrame);
  assert(decoded.value_count == 0 && decoded.date_count == 0);
  assert(decode_configured_plain_frame(official_water, 13,
      "02410120", "mkradio4", captures, 12, decoded)
      == PureDecodeStatus::InvalidFrame);
  assert(decoded.value_count == 0 && decoded.date_count == 0);
  // A valid radio header with a transport CI must never enter the
  // manufacturer-specific plain decoder, even when meter ID matches.
  uint8_t transport_frame[sizeof(official_water)]{};
  std::memcpy(transport_frame, official_water, sizeof(official_water));
  transport_frame[10] = 0x72;
  assert(decode_configured_plain_frame(transport_frame, sizeof(transport_frame),
      "02410120", "mkradio4", captures, 12, decoded)
      == PureDecodeStatus::UnsupportedTransport);
  assert(decoded.value_count == 0 && decoded.date_count == 0);
  transport_frame[10] = 0x7A;
  assert(decode_configured_plain_frame(transport_frame, sizeof(transport_frame),
      "02410120", "mkradio4", captures, 12, decoded)
      == PureDecodeStatus::UnsupportedTransport);
  assert(decoded.value_count == 0 && decoded.date_count == 0);
  transport_frame[10] = 0xA2;
  assert(decode_configured_plain_frame(transport_frame, sizeof(transport_frame),
      "02410120", "mkradio4", captures, 12, decoded)
      == PureDecodeStatus::Ok);
  // Isolated Mode-5 plaintext handoff fixture. The AES layer has already
  // verified the 2F 2F marker; the application bytes use the known water
  // driver fixture. This is NOT a claim of live Mode-5 water compatibility.
  const uint8_t mode5_water_plain[] = {
    0x2F,0x2F,0x06,0x70,0x29,0x01,0x00,0x60,0x17,
    0x03,0x00,0x04,0x00,0x03,0x00,0x00,0x00
  };
  assert(xmq_portable::decode_mode5_plain_payload(
      *water, mode5_water_plain, sizeof(mode5_water_plain),
      captures, 12, decoded) == xmq_portable::DriverStatus::Ok);
  bool mode5_target = false, mode5_total = false;
  for (size_t i=0; i<decoded.value_count; ++i) {
    if (!decoded.values[i].name) continue;
    if (std::strcmp(decoded.values[i].name,"target")==0) {
      assert(std::fabs(decoded.values[i].value-0.1)<1e-6);
      mode5_target=true;
    } else if (std::strcmp(decoded.values[i].name,"total")==0) {
      assert(std::fabs(decoded.values[i].value-0.4)<1e-6);
      mode5_total=true;
    }
  }
  assert(mode5_target && mode5_total);
  assert(xmq_portable::decode_mode5_plain_payload(
      *water, mode5_water_plain, 2, captures, 12, decoded)
      == xmq_portable::DriverStatus::BadArguments);
  assert(decoded.value_count==0 && decoded.date_count==0);
  uint8_t wrong_marker[sizeof(mode5_water_plain)]{};
  std::memcpy(wrong_marker,mode5_water_plain,sizeof(wrong_marker));
  wrong_marker[0]=0x00;
  assert(xmq_portable::decode_mode5_plain_payload(
      *water, wrong_marker, sizeof(wrong_marker), captures, 12, decoded)
      == xmq_portable::DriverStatus::BadArguments);
  assert(decoded.value_count==0 && decoded.date_count==0);
  std::puts("XMQ decode catalog PASS");
}
