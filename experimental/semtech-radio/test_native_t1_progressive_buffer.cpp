#include "../../components/wmbus_native_sx126x/t1_progressive_buffer.h"
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <vector>
using namespace esphome::wmbus_native_sx126x::t1;
static std::vector<uint8_t> encode(const std::vector<uint8_t> &bytes) {
  constexpr uint8_t symbols[16] = {0x16,0x0D,0x0E,0x0B,0x1C,0x19,0x1A,0x13,
                                   0x2C,0x25,0x26,0x23,0x34,0x31,0x32,0x29};
  std::vector<uint8_t> result((bytes.size()*12+7)/8,0);
  size_t offset=0;
  for (uint8_t byte: bytes)
    for (uint8_t nibble: {static_cast<uint8_t>(byte>>4),static_cast<uint8_t>(byte&15)})
      for (int bit=5;bit>=0;--bit,++offset)
        result[offset/8] |= static_cast<uint8_t>(((symbols[nibble]>>bit)&1u) << (7-offset%8));
  return result;
}
int main() {
  std::vector<uint8_t> frame(16,0x44); frame[0]=11;
  const auto encoded=encode(frame);
  ProgressiveBuffer b;
  assert(b.feed(nullptr,0)==FeedStatus::NeedMore);
  assert(b.feed(encoded.data(),1)==FeedStatus::NeedMore);
  assert(b.feed(encoded.data()+1,1)==FeedStatus::NeedMore);
  assert(b.expected_size()==encoded.size());
  for(size_t i=2;i<encoded.size()-1;++i)
    assert(b.feed(encoded.data()+i,1)==FeedStatus::NeedMore);
  assert(b.feed(encoded.data()+encoded.size()-1,1)==FeedStatus::Complete);
  assert(b.bytes()==encoded);
  assert(b.feed(encoded.data(),1)==FeedStatus::Rejected);
  b.reset();
  assert(b.feed(encoded.data(),encoded.size())==FeedStatus::Complete);
  b.reset();
  assert(b.feed(nullptr,1)==FeedStatus::Rejected);
  b.reset();
  const uint8_t bad[]={0,0,0};
  assert(b.feed(bad,sizeof(bad))==FeedStatus::Rejected);
  b.reset();
  assert(b.feed(encoded.data(),encoded.size()-1)==FeedStatus::NeedMore);
  assert(b.feed(encoded.data()+encoded.size()-1,1)==FeedStatus::Complete);
  // Exercise every 1..32-byte chunk size across several CRC-block lengths.
  for (unsigned l: {10u, 11u, 25u, 26u, 41u, 42u, 100u, 200u}) {
    const size_t blocks = 1 + ((static_cast<size_t>(l) - 9 + 15) / 16);
    std::vector<uint8_t> dll(static_cast<size_t>(l) + 1 + 2*blocks, 0x44);
    dll[0] = static_cast<uint8_t>(l);
    const auto bits = encode(dll);
    for (size_t chunk=1;chunk<=32;++chunk) {
      ProgressiveBuffer collector;
      size_t pos=0;
      while(pos<bits.size()) {
        const size_t take = std::min(chunk,bits.size()-pos);
        const auto status = collector.feed(bits.data()+pos,take);
        pos+=take;
        assert(status == (pos == bits.size() ? FeedStatus::Complete : FeedStatus::NeedMore));
      }
      assert(collector.bytes()==bits);
      assert(collector.expected_size()==bits.size());
    }
  }
  return 0;
}
