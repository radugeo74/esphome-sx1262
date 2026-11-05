#pragma once
// HOST ONLY: Apator 162 public XMQ IXML field grammar subset.
// A field is identified by a tag and a fixed payload length. This table
// mirrors the upstream ixml 'field*' alternatives, not fixed telegram offsets.
// Status(7), optional FF pad and trailing checksum(2 or 4) are structural.
// Does NOT validate wireless DLL CRC or encryption. The caller must supply
// trusted already-clear manufacturer application after the TPL header.
// It intentionally does not publish RF readings.
#include <cstddef>
#include <cstdint>
namespace xmq_portable { namespace apator162 {
enum class Status : uint8_t {
  Ok, Invalid, MissingTotal, DuplicateTotal, UnknownField, Truncated
};
struct Reading { double total_m3{0}; };
inline uint32_t le32(const uint8_t *p) {
  return static_cast<uint32_t>(p[0])|
         (static_cast<uint32_t>(p[1])<<8)|
         (static_cast<uint32_t>(p[2])<<16)|
         (static_cast<uint32_t>(p[3])<<24);
}
// The numbers are from the original apator162.xmq IXML rules:
// word=2, triplet=3, quad=4. All lengths exclude the opcode itself.
inline int field_bytes(uint8_t tag) {
  switch(tag) {
    case 0x00: case 0x10: case 0x42: case 0xA0: case 0xA1:
    case 0xA4: case 0xF0: return 4;
    case 0x01: case 0x44: case 0xA6: return 3;
    case 0x11: case 0x41: case 0x43: case 0xB4:
      return 2;
    case 0x40: case 0x8B: case 0x8C: return 6;
    case 0x8E: return 7;
    case 0x8A: return 9;
    case 0xA2: case 0xA5: case 0xA9: case 0xAF: return 1;
    case 0xA3: return 7;
    case 0xB0: return 5;
    case 0xB1: case 0xB3: return 8;
    case 0xB2: case 0xB5: return 16;
    default: break;
  }
  if(tag>=0x70 && tag<=0x7C)
    return 1+4*(static_cast<int>(tag)-0x70+1);
  switch(tag) {
    case 0x80:case 0x81:case 0x82:case 0x83:case 0x84:
    case 0x86:case 0x87:return 10;
    case 0x85:case 0x88:case 0x8F:return 11;
    case 0xA7:case 0xA8:case 0xAA:case 0xAB:case 0xAC:
    case 0xAD:return 2;
    case 0xB6:case 0xB7:case 0xB8:case 0xB9:case 0xBA:
    case 0xBB:case 0xBC:case 0xBD:case 0xBE:case 0xBF:
    case 0xC0:case 0xC1:case 0xC2:case 0xC3:case 0xC4:
    case 0xC5:case 0xC6:case 0xC7:
    case 0xD0:case 0xD3:return 3;
    default:return -1;
  }
}
inline Status decode_clear_manufacturer(const uint8_t *app,size_t size,
                                         Reading &out) {
  out={};
  // Original 'decode = -byte, status, field*, pad_FF?, checksum'.
  if(!app || size<1+7+2) return Status::Truncated;
  size_t pos=1+7;
  bool found=false;
  uint32_t total=0;
  while(pos<size) {
    const uint8_t tag=app[pos];
    if(tag==0xFF) break;
    const int data_bytes=field_bytes(tag);
    if(data_bytes<0) break;
    const size_t count=static_cast<size_t>(data_bytes);
    if(size-pos<=count) return Status::Truncated;
    if(tag==0x10) {
      if(found) return Status::DuplicateTotal;
      found=true;
      total=le32(app+pos+1);
    }
    pos+=1+count;
  }
  // Optional FF padding followed by a 2- or 4-byte checksum. The original
  // XMQ consumes it but does not provide a checksum verification algorithm.
  const size_t rest=size-pos;
  bool valid_tail=false;
  for(size_t csum : {size_t(2),size_t(4)}) {
    if(rest<csum) continue;
    bool padding=true;
    for(size_t i=pos;i<size-csum;++i)
      if(app[i]!=0xFF) {padding=false;break;}
    if(padding) {valid_tail=true;break;}
  }
  if(!valid_tail) return Status::UnknownField;
  if(!found) return Status::MissingTotal;
  out.total_m3=static_cast<double>(total)/1000.0;
  return Status::Ok;
}
}} // namespace xmq_portable::apator162
