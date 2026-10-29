#include "../../../components/wmbus_radio/pure_complete_frame.h"
#include <cassert>
#include <vector>
using namespace wmbus_pure;
static void crc_append(std::vector<uint8_t> &v, size_t begin, size_t n) {
  auto crc=crc16_en13757(v.data()+begin,n);
  v.push_back(static_cast<uint8_t>(crc>>8));
  v.push_back(static_cast<uint8_t>(crc));
}
int main() {
  // Build a valid frame A with a real TCH manufacturer, BCD address and
  // multiple CRC blocks, then exercise corruption and truncated inputs.
  std::vector<uint8_t> raw{0x32,0x44,0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80};
  crc_append(raw,0,10);
  for (unsigned i=0;i<41;) {
    unsigned n=(41-i)<16?(41-i):16;
    size_t start=raw.size();
    for(unsigned j=0;j<n;++j) raw.push_back(static_cast<uint8_t>(i+j));
    crc_append(raw,start,n);
    i+=n;
  }
  CompleteFrame result{};
  assert(verify_complete_frame(raw.data(),raw.size(),CrcFrameFormat::A,result)==CompleteStatus::Ok);
  assert(result.bytes.size()==51 && result.header.meter_id==24273067);
  assert(result.header.manufacturer==0x5068 && result.header.application_length==40);
  assert(result.header.application==result.bytes.data()+11);
  auto bad=raw;
  bad[15]^=1;
  assert(verify_complete_frame(bad.data(),bad.size(),CrcFrameFormat::A,result)==CompleteStatus::BadCrc);
  assert(result.bytes.empty() && result.header.application==nullptr);
  assert(verify_complete_frame(raw.data(),raw.size()-1,CrcFrameFormat::A,result)==CompleteStatus::InvalidLength);
  assert(result.bytes.empty());
  assert(verify_complete_frame(nullptr,0,CrcFrameFormat::A,result)==CompleteStatus::TooShort);

  // Format B, first and only CRC at the end.
  std::vector<uint8_t> b{0x0C,0x44,0x68,0x50,0x67,0x30,0x27,0x24,0x69,0x80,0xA0};
  crc_append(b,0,b.size());
  assert(verify_complete_frame(b.data(),b.size(),CrcFrameFormat::B,result)==CompleteStatus::Ok);
  assert(result.header.meter_id==24273067);
  b[7]^=1;
  assert(verify_complete_frame(b.data(),b.size(),CrcFrameFormat::B,result)==CompleteStatus::BadCrc);

  // Correct CRC but invalid manufacturer must not become a supported header.
  auto invalid_header=raw;
  invalid_header[2]=0;invalid_header[3]=0;
  crc_append(invalid_header,0,10); // Rebuild the first block CRC below.
  invalid_header[10]=invalid_header[invalid_header.size()-2];
  invalid_header[11]=invalid_header[invalid_header.size()-1];
  invalid_header.resize(raw.size());
  assert(verify_complete_frame(invalid_header.data(),invalid_header.size(),
                               CrcFrameFormat::A,result)==CompleteStatus::UnsupportedHeader);
}
