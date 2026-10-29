#include "../../../components/wmbus_radio/pure_dll_crc.h"
#include <cassert>
#include <vector>
using namespace wmbus_pure;
static void append_crc(std::vector<uint8_t> &frame, size_t begin, size_t size) {
  const auto crc = crc16_en13757(frame.data()+begin,size);
  frame.push_back(static_cast<uint8_t>(crc >> 8));
  frame.push_back(static_cast<uint8_t>(crc));
}
int main() {
  const uint8_t sample[]{0x32,0x44,0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80,0xA0};
  std::vector<uint8_t> a(sample,sample+10);
  append_crc(a,0,10);
  const size_t count = 51-10;
  for (size_t i=0;i<count;) {
    const size_t n=(count-i)<16?(count-i):16;
    const size_t begin=a.size();
    for(size_t j=0;j<n;++j) a.push_back(static_cast<uint8_t>(i+j));
    append_crc(a,begin,n);
    i+=n;
  }
  // 51 clean bytes, 4 CRC blocks, 59 on-wire bytes.
  assert(a.size()==59);
  std::vector<uint8_t> decoded{0xAA};
  assert(strip_dll_crc(a.data(),a.size(),CrcFrameFormat::A,decoded)==CrcStatus::Ok);
  assert(decoded.size()==51 && decoded[0]==50 && decoded[1]==0x44);
  auto bad=a; bad[13]^=0x01;
  decoded={0xAA};
  assert(strip_dll_crc(bad.data(),bad.size(),CrcFrameFormat::A,decoded)==CrcStatus::BadCrc);
  assert(decoded.size()==1 && decoded[0]==0xAA);
  assert(strip_dll_crc(a.data(),a.size()-1,CrcFrameFormat::A,decoded)==CrcStatus::LengthMismatch);

  std::vector<uint8_t> b{0x0C,0x44,0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80,0xA0};
  append_crc(b,0,b.size());
  assert(b.size()==13);
  assert(strip_dll_crc(b.data(),b.size(),CrcFrameFormat::B,decoded)==CrcStatus::Ok);
  assert(decoded.size()==11 && decoded[0]==10);
  b[11]^=0x01;
  assert(strip_dll_crc(b.data(),b.size(),CrcFrameFormat::B,decoded)==CrcStatus::BadCrc);
  // Captured Heltec SX1262 C1-A frame from 2026-10-29. Legacy accepted
  // the untrimmed bytes as a telegram; the DLL CRC is actually invalid.
  // Preserve this as a regression: never treat it as a verified payload.
  const char *hex_c1 = "364468505038412295373AD972985239732423FEE7CA51204B258FAF8BE525B1F2F6D9146C0230A5F0C6DA48E47DB3E680483E6AF0506A3C3FB58BA9138E12";
  std::vector<uint8_t> captured;
  for (const char *p=hex_c1; *p; p+=2) {
    auto nibble=[](char c)->uint8_t { return c<='9' ? static_cast<uint8_t>(c-'0') : static_cast<uint8_t>(c-'A'+10); };
    captured.push_back(static_cast<uint8_t>((nibble(p[0])<<4)|nibble(p[1])));
  }
  decoded={0xAA};
  assert(strip_dll_crc(captured.data(),captured.size(),CrcFrameFormat::A,decoded)==CrcStatus::BadCrc);
  assert(decoded.size()==1 && decoded[0]==0xAA);

}
