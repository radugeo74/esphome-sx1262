#include "components/wmbus_native_sx126x/minomess_clear_prefix.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>
static std::vector<uint8_t> unhex(const char *p) {
  std::vector<uint8_t> out;
  while (*p) {
    const auto digit=[](char c)->uint8_t {
      if (c>='0'&&c<='9') return c-'0';
      if (c>='A'&&c<='F') return c-'A'+10;
      assert(false); return 0;
    };
    const auto a=digit(*p++); assert(*p);
    const auto b=digit(*p++);
    out.push_back(static_cast<uint8_t>((a<<4)|b));
  }
  return out;
}
int main() {
  using namespace xmq_portable::minomess;
  Reading reading{};
  // These are the first standard/rewritten records from unmodified public
  // minomess.xmq post-decryption fixtures. No real user RF data.
  auto absent=unhex("0C1359000000026CBE2B82046CA12B8C0413FFFFFFFF");
  assert(decode_clear_prefix(absent.data(),absent.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-0.059)<1e-8);
  assert(!reading.has_target);
  assert(reading.has_meter_date && reading.has_target_date);
  assert(reading.meter_date.year==2021 && reading.meter_date.month==11 &&
         reading.meter_date.day==30);
  assert(reading.target_date.year==2021 && reading.target_date.month==11 &&
         reading.target_date.day==1);

  auto cold=unhex("0C1355000000026CEC2182046CE1218C041300000080");
  assert(decode_clear_prefix(cold.data(),cold.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-0.055)<1e-8);
  assert(!reading.has_target); // -2147483.648 is the upstream null sentinel.

  auto warm=unhex("0C1357000000026CEC2182046CE1218C041300000080");
  assert(decode_clear_prefix(warm.data(),warm.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-0.057)<1e-8 && !reading.has_target);

  auto populated=unhex("0C1354031400026C4F3A82046C413A8C0413F51C0200");
  assert(decode_clear_prefix(populated.data(),populated.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-140.354)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-138.485)<1e-8);
  assert(reading.meter_date.year==2026 && reading.meter_date.month==10 &&
         reading.meter_date.day==15);
  assert(reading.target_date.year==2026 && reading.target_date.month==10 &&
         reading.target_date.day==1);

  // Full public minomess.xmq RF envelope. Upstream NOKEY means the
  // application following '_' is supplied clear after decryption; its
  // actual TPL configuration (0x0550) is MODE 5. Reject it as live RF.
  auto upstream_rf=unhex(
    "6644496A7856341218377278563412496A01067D0050052F2F"
    "0C1354031400026C4F3A82046C413A8C0413F51C0200"
    "8D0493132C33FEEE0F020D05021EFA018CED010CE0013ED401"
    "FFC601A9B701E2A2011B94014884011D7401FC6601EA5A01"
    "02FD1700002F2F");
  assert(decode_clear_wireless_frame(upstream_rf.data(),
                                    upstream_rf.size(),reading)==Status::Unsupported);
  assert(reading.total_m3==0 && !reading.has_target);
  // Only a synthetic, MODE ZERO version may exercise the clear TPL path.
  // The DLL radio version differs from the embedded TPL version; match
  // the latter for identification, but DO NOT authorize publication.
  upstream_rf[22]=0x00;
  assert(decode_clear_wireless_frame(upstream_rf.data(),
                                    upstream_rf.size(),reading)==Status::Ok);
  assert(std::fabs(reading.total_m3-140.354)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-138.485)<1e-8);
  assert(reading.status_bits==0); // 02FD17 from the upstream fixture
  // Preserve strict verification of the whole application; a correct
  // first 22 bytes do not authorize a truncated or corrupt history tail.
  auto bad_tail=upstream_rf;
  bad_tail[25+22+4]=0xFF; // wrong monthly-history LVAR length
  assert(decode_clear_wireless_frame(bad_tail.data(),bad_tail.size(),reading)
         ==Status::Unsupported);
  assert(reading.total_m3==0 && !reading.has_target);
  bad_tail=upstream_rf;
  bad_tail[25+22+5+44+2]=0x18; // 02FD17 status identity corrupted
  assert(decode_clear_wireless_frame(bad_tail.data(),bad_tail.size(),reading)
         ==Status::MissingFields);
  assert(reading.total_m3==0);
  bad_tail=upstream_rf;
  bad_tail.resize(bad_tail.size()-4); // truncate the mandatory status
  bad_tail[0]=static_cast<uint8_t>(bad_tail.size()-1);
  assert(decode_clear_wireless_frame(bad_tail.data(),bad_tail.size(),reading)
         !=Status::Ok); // missing complete trailing DIF/VIF record or pad
  assert(reading.total_m3==0);

  upstream_rf[18]=0x12; // TPL version is now not among official detects.
  assert(decode_clear_wireless_frame(upstream_rf.data(),
                                    upstream_rf.size(),reading)==Status::Unsupported);
  assert(reading.total_m3==0 && !reading.has_target);

  // All four official wireless Minomess golden fixtures, including
  // both upstream missing-target encodings. Their original mode 5 is
  // rejected; only synthetic mode zero exercises clear record parsing.
  {
    auto frame=unhex("6644496A1064035514377251345015496A0007EE0050052F2F0C1359000000026CBE2B82046CA12B8C0413FFFFFFFF8D0493132CFBFEFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF02FD1700002F2F");
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Unsupported);
    assert(reading.total_m3==0);
    frame[22]=0; // synthetic clear transport; never mutate the XMQ source
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Ok);
    assert(std::fabs(reading.total_m3-0.059)<1e-8);
    assert(!reading.has_target);
    frame=unhex("6644496A4425155518377251413121496A0116360050052F2F0C1355000000026CEC2182046CE1218C0413000000808D0493132C33FE00008000008000008000008000008000008000008000008000008000008000008000008000008000008002FD1700002F2F");
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Unsupported);
    assert(reading.total_m3==0);
    frame[22]=0; // synthetic clear transport; never mutate the XMQ source
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Ok);
    assert(std::fabs(reading.total_m3-0.055)<1e-8);
    assert(!reading.has_target);
    frame=unhex("6644496A8753155518377221314151496A0106300050052F2F0C1357000000026CEC2182046CE1218C0413000000808D0493132C33FE00008000008000008000008000008000008000008000008000008000008000008000008000008000008002FD1700002F2F");
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Unsupported);
    assert(reading.total_m3==0);
    frame[22]=0; // synthetic clear transport; never mutate the XMQ source
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Ok);
    assert(std::fabs(reading.total_m3-0.057)<1e-8);
    assert(!reading.has_target);
    frame=unhex("6644496A7856341218377278563412496A01067D0050052F2F0C1354031400026C4F3A82046C413A8C0413F51C02008D0493132C33FEEE0F020D05021EFA018CED010CE0013ED401FFC601A9B701E2A2011B94014884011D7401FC6601EA5A0102FD1700002F2F");
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Unsupported);
    assert(reading.total_m3==0);
    frame[22]=0; // synthetic clear transport; never mutate the XMQ source
    assert(decode_clear_wireless_frame(frame.data(),frame.size(),reading)
           ==Status::Ok);
    assert(std::fabs(reading.total_m3-140.354)<1e-8);
    assert(reading.has_target && std::fabs(reading.target_m3-138.485)<1e-8);
  }

  // Malformed DIF, invalid BCD and truncated value never keep old readings.
  populated[0]=0x0D;
  assert(decode_clear_prefix(populated.data(),populated.size(),reading)
         ==Status::Unsupported);
  assert(reading.total_m3==0 && !reading.has_target);
  populated[0]=0x0C; populated[2]=0xFA;
  assert(decode_clear_prefix(populated.data(),populated.size(),reading)
         ==Status::InvalidBcd);
  assert(reading.total_m3==0);
  populated[2]=0x54;
  assert(decode_clear_prefix(populated.data(),21,reading)==Status::Truncated);
  auto invalid_date=populated; invalid_date[8]=0;
  assert(decode_clear_prefix(invalid_date.data(),invalid_date.size(),reading)
         ==Status::InvalidDate);
  return 0;
}
