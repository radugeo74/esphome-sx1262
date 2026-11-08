// Host integration: SAME verified native ingress + configured secure
// delivery gate and field-map callbacks as NativeReceiverProbe.
// Uses public upstream ciphertext (Apator162) and independently AES-
// encrypted public already-clear XMQ fixture applications (Topas/MK).
// No real meter keys, no RF hardware, no actual Home Assistant server.
#include "components/wmbus_native_sx126x/native_verified_frame.h"
#include "components/wmbus_native_sx126x/native_meter_registry.h"
#include "components/wmbus_native_sx126x/secure_meter_delivery.h"
#include "components/wmbus_native_sx126x/passive_auto_identify.h"
#include "xmq_auto_registry.h"
#include <mbedtls/aes.h>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace native=esphome::wmbus_native_sx126x;
using native::boundary::Mode;
using native::verified::Status;
using native::secure_delivery::Outcome;

static std::vector<uint8_t> hex(const char *s) {
  std::vector<uint8_t> output;int high=-1;
  for(;*s;++s) {
    if(*s=='_' || *s==' ' || *s=='\n') continue;
    const int value=(*s>='0'&&*s<='9')?*s-'0':
        (*s>='a'&&*s<='f')?*s-'a'+10:
        (*s>='A'&&*s<='F')?*s-'A'+10:-1;
    assert(value>=0);
    if(high<0) high=value;
    else { output.push_back(static_cast<uint8_t>((high<<4)|value));high=-1; }
  }
  assert(high<0);return output;
}
static void append_crc(std::vector<uint8_t> &out,
                       const uint8_t *data,size_t count) {
  const auto crc=wmbus_pure::crc16_en13757(data,count);
  out.push_back(static_cast<uint8_t>(crc>>8));
  out.push_back(static_cast<uint8_t>(crc));
}
// Real format-A DLL transport: CRC after the 10-byte first block and
// after each following 16-byte (or last partial) block.
static std::vector<uint8_t> format_a(const std::vector<uint8_t> &plain) {
  assert(plain.size()>10 && plain.size()==size_t(plain[0])+1);
  std::vector<uint8_t> out;out.reserve(plain.size()+20);
  size_t pos=0;
  while(pos<plain.size()) {
    const size_t count=pos==0?10:
       std::min<size_t>(16,plain.size()-pos);
    out.insert(out.end(),plain.begin()+pos,plain.begin()+pos+count);
    append_crc(out,plain.data()+pos,count);
    pos+=count;
  }
  return out;
}
// Real format-B DLL transport, where L includes final CRC bytes.
static std::vector<uint8_t> format_b(const std::vector<uint8_t> &plain) {
  assert(plain.size()>10 && plain.size()<126 &&
         plain.size()==size_t(plain[0])+1);
  auto out=plain;
  out[0]=static_cast<uint8_t>(out.size()+1); // 2 CRC bytes
  append_crc(out,out.data(),out.size());
  return out;
}
static std::vector<uint8_t> encode_t1(const std::vector<uint8_t> &dll) {
  static constexpr uint8_t symbol[]={
    0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
    0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29
  };
  std::vector<uint8_t> result((dll.size()*12+7)/8,0);
  size_t bit=0;
  for(const uint8_t value:dll) {
    for(uint8_t nibble : {static_cast<uint8_t>(value>>4),
                          static_cast<uint8_t>(value&15u)}) {
      for(int shift=5;shift>=0;--shift,++bit)
        result[bit/8] |= static_cast<uint8_t>(
            ((symbol[nibble]>>shift)&1u)<<(7-bit%8));
    }
  }
  return result;
}
static native::verified::Result verify(
    const std::vector<uint8_t> &original,Mode mode) {
  if(mode==Mode::C1B) {
    const auto air=format_b(original);
    return native::verified::process(mode,air.data(),air.size());
  }
  const auto air=format_a(original);
  if(mode==Mode::C1A)
    return native::verified::process(mode,air.data(),air.size());
  const auto t1=encode_t1(air);
  return native::verified::process(mode,t1.data(),t1.size());
}
static std::string sender_id(uint32_t number) {
  char str[9]{};
  std::snprintf(str,sizeof(str),"%08u",static_cast<unsigned>(number));
  return str;
}
// Mirror the firmware's configured-sensor mapping boundaries. emit_mapped
// is production code, not duplicated here; only the HA sensors are mocks.
struct SimulatedSensors {
  std::string configured_id;
  std::map<std::string,double> numbers;
  std::map<std::string,std::string> texts;
  std::vector<std::string> enabled_number_fields;
  std::vector<std::string> enabled_text_fields;
  size_t rssi_count{0};
  float last_rssi{0};
  size_t event_count() const {
    return numbers.size()+texts.size()+rssi_count;
  }
  void accept(const native::secure_delivery::Result &result,float rssi) {
    if(!result.binding || result.binding->id!=configured_id) return;
    native::secure_delivery::emit_mapped(result,rssi,
      [&](const char *field,double value) {
        for(const auto &name:enabled_number_fields)
          if(name==field) numbers[name]=value;
      },
      [&](const char *field,const char *value) {
        for(const auto &name:enabled_text_fields)
          if(name==field) texts[name]=value;
      },
      [&](float value) {++rssi_count;last_rssi=value;});
  }
};
struct PipelineResult {
  Status ingress{Status::BadBoundary};
  native::registry::Kind registry{native::registry::Kind::Invalid};
  Outcome delivery{Outcome::Unmatched};
};
static PipelineResult pass(const native::verified::Result &frame,
     const std::vector<native::secure_bindings::Entry> &entries,
     SimulatedSensors &sensors) {
  PipelineResult p{};
  p.ingress=frame.status;
  if(frame.status!=Status::Ok) return p;
  std::vector<std::string> configured_senders;
  for(const auto &entry:entries) configured_senders.push_back(entry.dll_id);
  p.registry=native::registry::classify(frame,configured_senders);
  if(p.registry!=native::registry::Kind::Configured) return p;
  native::secure_delivery::Result decoded{};
  p.delivery=native::secure_delivery::evaluate(
      frame.frame.bytes.data(),frame.frame.bytes.size(),
      sender_id(frame.frame.header.meter_id),entries,decoded);
  if(p.delivery==Outcome::Ready) sensors.accept(decoded,-72.5f);
  return p;
}
static void expect_number(const SimulatedSensors &s,const char *field,
                          double expected) {
  const auto it=s.numbers.find(field);
  assert(it!=s.numbers.end() && std::fabs(it->second-expected)<0.00001);
}
static std::vector<uint8_t> encrypt_topas(
    const std::vector<uint8_t> &clear,const uint8_t key[16]) {
  assert(clear.size()==79 && clear[15]==0x2F && clear[16]==0x2F);
  auto ciphertext=clear;
  uint8_t iv[16]{};
  std::memcpy(iv,clear.data()+2,8);
  std::memset(iv+8,clear[11],8);
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  assert(mbedtls_aes_setkey_enc(&aes,key,128)==0);
  assert(mbedtls_aes_crypt_cbc(&aes,MBEDTLS_AES_ENCRYPT,64,iv,
      clear.data()+15,ciphertext.data()+15)==0);
  mbedtls_aes_free(&aes);
  return ciphertext;
}

int main() {
  // 1. Real upstream encrypted APA public fixture #19: full 3-of-6 T1
  // PHY payload -> format-A DLL CRC -> configured AES -> mapped sensor.
  const auto apator=hex(
      "3e4401069620280105077a7400308588f90b4dbd012bd789d7b4fb8536"
      "b889425b8624aee20a4862e60223427e24a9060d0db7b112cdd42a0"
      "a825130f3858a");
  std::vector<native::secure_bindings::Entry> entries;
  const std::string zero_key(32,'0');
  assert(native::secure_bindings::add(entries,"01282096","apator162",
                                     zero_key,"01282096"));
  SimulatedSensors apator_sensors{
      "01282096",{}, {}, {"total_m3","unmapped"}, {}};
  auto t1=verify(apator,Mode::T1);
  assert(t1.status==Status::Ok && t1.frame.bytes==apator);
  auto outcome=pass(t1,entries,apator_sensors);
  assert(outcome.registry==native::registry::Kind::Configured);
  assert(outcome.delivery==Outcome::Ready);
  expect_number(apator_sensors,"total_m3",7.512);
  assert(apator_sensors.numbers.count("unmapped")==0);
  assert(apator_sensors.rssi_count==1);
  // The same encrypted application in C1-B uses a different DLL CRC
  // envelope but the SAME verified configured decoder and HA field sink.
  SimulatedSensors apator_c1{
      "01282096",{}, {}, {"total_m3"}, {}};
  const auto c1=verify(apator,Mode::C1B);
  assert(c1.status==Status::Ok && c1.frame.bytes==apator);
  assert(pass(c1,entries,apator_c1).delivery==Outcome::Ready);
  expect_number(apator_c1,"total_m3",7.512);
  assert(apator_c1.rssi_count==1);
  // Valid format-A C1 with same Apator ciphertext must work as well.
  SimulatedSensors apator_c1a{
      "01282096",{}, {}, {"total_m3"}, {}};
  assert(pass(verify(apator,Mode::C1A),entries,apator_c1a).delivery==
         Outcome::Ready);
  expect_number(apator_c1a,"total_m3",7.512);
  // A frame with broken DLL CRC must not even reach configured decoding.
  auto damaged_air=format_b(apator);
  damaged_air[17]^=1;
  const auto bad=native::verified::process(
      Mode::C1B,damaged_air.data(),damaged_air.size());
  assert(bad.status!=Status::Ok && bad.frame.bytes.empty());
  SimulatedSensors blocked_crc{"01282096",{}, {}, {"total_m3"}, {}};
  assert(pass(bad,entries,blocked_crc).delivery==Outcome::Unmatched);
  assert(blocked_crc.event_count()==0);
  // Correct physical frame, WRONG CONFIGURED KEY -> no numeric, text or RSSI.
  auto wrong_key=entries;
  wrong_key[0].key[0]^=0x80;
  SimulatedSensors blocked_key{"01282096",{}, {}, {"total_m3"}, {}};
  assert(pass(t1,wrong_key,blocked_key).delivery==Outcome::Rejected);
  assert(blocked_key.event_count()==0);
  // Wrong configured target: radio frame remains discovered, no bindings.
  auto wrong_id=entries;
  wrong_id[0].id="01282097";
  wrong_id[0].dll_id="01282097";
  SimulatedSensors blocked_id{"01282097",{}, {}, {"total_m3"}, {}};
  outcome=pass(t1,wrong_id,blocked_id);
  assert(outcome.registry==native::registry::Kind::Discovered);
  assert(outcome.delivery==Outcome::Unmatched && blocked_id.event_count()==0);
  // Correct DLL + wrong nested model version with CRC refreshed
  // still cannot yield a reading (don't interpret ciphertext as clear).
  auto wrong_model=apator;
  wrong_model[9]=0x08;
  SimulatedSensors blocked_model{"01282096",{}, {}, {"total_m3"}, {}};
  assert(pass(verify(wrong_model,Mode::C1B),entries,blocked_model).delivery==
         Outcome::Rejected);
  assert(blocked_model.event_count()==0);
  auto wrong_mode=apator;
  wrong_mode[14]=0x00;  // M=0 is not an AES Mode-5 packet
  SimulatedSensors blocked_mode{"01282096",{}, {}, {"total_m3"}, {}};
  assert(pass(verify(wrong_mode,Mode::C1B),entries,blocked_mode).delivery==
         Outcome::Rejected);
  assert(blocked_mode.event_count()==0);

  // 2. Topas original WMBusmeters NOKEY application is POST-DECRYPTED.
  // Re-encrypt with synthetic public test key; only ciphertext is
  // accepted by the live SECURE_ONLY path.
  const auto topas_clear=hex(
      "4E44B40512345678F1077A310040052F2F_"
      "01FD08040C13991848004C1359423500CC101300000000CC201359423500"
      "426C7F2C0B3B00000002FD74DA10025AD300C4016D3B179F27CC011387124600");
  const uint8_t topas_key[16]={
      1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
  const auto topas_cipher=encrypt_topas(topas_clear,topas_key);
  std::vector<native::secure_bindings::Entry> topas_bindings;
  assert(native::secure_bindings::add(
      topas_bindings,"78563412","topaseskr",
      "0102030405060708090A0B0C0D0E0F10","78563412"));
  SimulatedSensors topas_sensors{"78563412",{}, {},
      {"total_m3","temperature_c","current_flow_m3h",
       "volume_year_period_m3","unmapped"}, {}};
  assert(pass(verify(topas_cipher,Mode::C1B),topas_bindings,
              topas_sensors).delivery==Outcome::Ready);
  expect_number(topas_sensors,"total_m3",481.899);
  expect_number(topas_sensors,"temperature_c",21.1);
  expect_number(topas_sensors,"current_flow_m3h",0);
  expect_number(topas_sensors,"volume_year_period_m3",354.259);
  assert(topas_sensors.numbers.count("unmapped")==0);
  assert(topas_sensors.rssi_count==1);
  // Missing key/entry: passive driver identification is NOT permission
  // to decode or publish, even with valid CRC and matching MVT.
  const auto topas_verified=verify(topas_cipher,Mode::C1B);
  const auto candidate=wmbus_pure::passive_auto::inspect(
      topas_verified.frame.bytes.data(),topas_verified.frame.bytes.size(),
      wmbus_pure::generated::signatures,
      wmbus_pure::generated::signature_count);
  assert(candidate.state==wmbus_pure::batch2_identify::State::Candidate);
  assert(candidate.driver && std::strcmp(candidate.driver,"topaseskr")==0);
  SimulatedSensors not_configured{"78563412",{}, {}, {"total_m3"}, {}};
  assert(pass(topas_verified,{},not_configured).delivery==Outcome::Unmatched);
  assert(not_configured.event_count()==0);
  SimulatedSensors post_decrypted{"78563412",{}, {}, {"total_m3"}, {}};
  assert(pass(verify(topas_clear,Mode::C1B),
              topas_bindings,post_decrypted).delivery==Outcome::Rejected);
  assert(post_decrypted.event_count()==0);
  SimulatedSensors wrong_topas_key{"78563412",{}, {}, {"total_m3"}, {}};
  auto wrong_topas=topas_bindings;
  wrong_topas[0].key[15]^=0x80;
  assert(pass(topas_verified,wrong_topas,wrong_topas_key).delivery==
         Outcome::Rejected);
  assert(wrong_topas_key.event_count()==0);

  // 3. Actual delivery logic must map MK Radio4a target_date only AFTER
  // successful AES + converter DLL/TPL match. Public XMQ fixture records
  // have been independently encrypted with the synthetic key.
  const auto mk=hex(
      "4B44685036494600953772253895662423FE064E003005"
      "DF5266AD7770799E99223CADB3EBD2144CD83D771F47A1DC0645E1D42F"
      "291D89ED6E6449DEC640AC2C637FFB8F26BB242F2F2F2F2F");
  std::vector<native::secure_bindings::Entry> mk_bindings;
  assert(native::secure_bindings::add(
      mk_bindings,"66953825","mkradio4a",
      "000102030405060708090A0B0C0D0E0F","00464936"));
  SimulatedSensors mk_sensors{"66953825",{}, {},
      {"target_m3"},{"target_date","status"}};
  assert(pass(verify(mk,Mode::C1B),mk_bindings,mk_sensors).delivery==
         Outcome::Ready);
  expect_number(mk_sensors,"target_m3",16.1);
  assert(mk_sensors.texts.at("target_date")=="2021-12-31");
  assert(mk_sensors.texts.count("status")==0);
  assert(mk_sensors.rssi_count==1);
  SimulatedSensors mk_bad_tpl{"66953825",{}, {},
      {"target_m3"},{"target_date"}};
  auto changed=mk;
  changed[17]=0x01; // wrong HYD version; CRC remains VALID after rewrap
  assert(pass(verify(changed,Mode::C1B),mk_bindings,mk_bad_tpl).delivery==
         Outcome::Rejected);
  assert(mk_bad_tpl.event_count()==0);
  std::puts("PASS verified T1/C1-A/C1-B -> secure bindings -> native AES "
            "-> exact mapped number/text/RSSI callbacks; original Apator "
            "and synthetic Topas/MK; bad CRC/key/ID/model/mode/unconfigured "
            "and post-decrypted NOKEY fail closed; no physical RF/HA.");
}
