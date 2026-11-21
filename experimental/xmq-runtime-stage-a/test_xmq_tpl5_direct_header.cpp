#include "xmq_tpl5_direct_header.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace xmq_runtime;
static void require(bool ok, const char *message) {
  if (!ok) throw std::runtime_error(message);
}
static std::vector<std::uint8_t> synth_cipher(
    const Tpl5HeaderFields &fields, const std::array<std::uint8_t,16> &key) {
  std::vector<std::uint8_t> clear(32, 0);
  clear[0]=0x2f; clear[1]=0x2f; clear[2]=0x11; clear[8]=0x33;
  std::vector<std::uint8_t> encrypted(32);
  auto iv=make_tpl5_iv(fields);
  AES_CBC_encrypt_buffer(encrypted.data(), clear.data(), 32,
                         key.data(), iv.data());
  return encrypted;
}
static std::vector<std::uint8_t> frame(std::uint8_t ci,
    const Tpl5HeaderFields &dll, const Tpl5HeaderFields &tpl,
    std::uint16_t cfg, const std::vector<std::uint8_t> &application) {
  std::vector<std::uint8_t> data{
      0,0x44,dll.mfct_le[0],dll.mfct_le[1]};
  data.insert(data.end(),dll.address_a.begin(),dll.address_a.end());
  data.push_back(ci);
  if(ci==0x72) {
    data.insert(data.end(),tpl.address_a.begin(),tpl.address_a.begin()+4);
    data.insert(data.end(),tpl.mfct_le.begin(),tpl.mfct_le.end());
    data.push_back(tpl.address_a[4]);
    data.push_back(tpl.address_a[5]);
  }
  data.push_back(ci==0x72 ? tpl.access : dll.access);
  data.push_back(0x00);  // TPL STS
  data.push_back(static_cast<std::uint8_t>(cfg&0xff));
  data.push_back(static_cast<std::uint8_t>(cfg>>8));
  data.insert(data.end(),application.begin(),application.end());
  data[0]=static_cast<std::uint8_t>(data.size()-1);
  return data;
}
int main() {
 try {
  const std::array<std::uint8_t,16> key{
    0x2b,0x7e,0x15,0x16,0x28,0xae,0xd2,0xa6,
    0xab,0xf7,0x15,0x88,0x09,0xcf,0x4f,0x3c};
  Tpl5HeaderFields dll{{0x34,0x12},{0x11,0x22,0x33,0x44,0x95,0x62},0x5B};
  Tpl5HeaderFields tpl{{0x56,0x78},{0x10,0x20,0x30,0x40,0x94,0x80},0x6C};
  const std::vector<std::uint8_t> tail{0x01,0x02,0x03};
  const auto short_cipher=synth_cipher(dll,key);
  auto short_app=short_cipher;
  short_app.insert(short_app.end(),tail.begin(),tail.end());
  auto short_frame=frame(0x7A,dll,tpl,0x0520,short_app);
  const auto parsed_short=parse_direct_tpl5_header(short_frame,true);
  require(parsed_short.valid && !parsed_short.inner_tpl_address &&
          parsed_short.fields.access==dll.access &&
          parsed_short.encrypted_blocks==2 &&
          parsed_short.payload_offset==15,
          "short TPL header parse failed");
  const auto short_output=decrypt_direct_tpl5_frame(short_frame,true,key);
  require(short_output.accepted &&
          short_output.plaintext.size()==short_app.size() &&
          short_output.plaintext[0]==0x2F &&
          short_output.plaintext[1]==0x2F &&
          short_output.plaintext[8]==0x33 &&
          short_output.plaintext.back()==0x03,
          "short encrypted payload / clear tail failed");

  const auto long_cipher=synth_cipher(tpl,key);
  auto long_frame=frame(0x72,dll,tpl,0x0520,long_cipher);
  const auto parsed_long=parse_direct_tpl5_header(long_frame,true);
  require(parsed_long.valid && parsed_long.inner_tpl_address &&
          parsed_long.payload_offset==23 &&
          parsed_long.fields.mfct_le==tpl.mfct_le &&
          parsed_long.fields.address_a==tpl.address_a &&
          parsed_long.fields.access==tpl.access,
          "long TPL inner address M/A/ACC extraction failed");
  const auto long_output=decrypt_direct_tpl5_frame(long_frame,true,key);
  require(long_output.accepted && long_output.plaintext[8]==0x33,
          "long TPL decrypt with inner address failed");
  require(!decrypt_direct_tpl5_frame(long_frame,false,key).accepted,
          "unverified DLL CRC accepted");
  auto corrupted_len=short_frame;
  ++corrupted_len[0];
  require(!parse_direct_tpl5_header(corrupted_len,true).valid,
          "corrupted DLL length accepted");
  auto unsupported_ci=short_frame;
  unsupported_ci[10]=0xA2;
  require(!parse_direct_tpl5_header(unsupported_ci,true).valid,
          "manufacturer-specific CI accepted as standard TPL");
  auto unsupported_ell=short_frame;
  unsupported_ell[10]=0x8C;
  require(!parse_direct_tpl5_header(unsupported_ell,true).valid,
          "ELL CI accepted without parsing ELL");
  auto unsupported_mode=short_frame;
  unsupported_mode[14]=0x07;
  require(!parse_direct_tpl5_header(unsupported_mode,true).valid,
          "TPL mode 7 accepted without MAC/KDF");
  auto short_blocks=short_frame;
  short_blocks[13]=0x30;
  require(!parse_direct_tpl5_header(short_blocks,true).valid,
          "insufficient encrypted blocks accepted");
  auto short_truncated=short_frame;
  short_truncated.resize(20);
  short_truncated[0]=static_cast<std::uint8_t>(short_truncated.size()-1);
  require(!parse_direct_tpl5_header(short_truncated,true).valid,
          "truncated TPL5 payload accepted");
  std::cout<<"XMQ_TPL5_DIRECT_HEADER_OK: CI7A/72, inner address, "
              "block count, CRC guard, unsupported CI and clear tail\n";
  return 0;
 } catch (const std::exception &e) {
  std::cerr<<"FAIL "<<e.what()<<'\n';
  return 1;
 }
}
