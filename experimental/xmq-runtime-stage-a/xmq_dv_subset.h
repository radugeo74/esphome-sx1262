#pragma once
// Stage A - bounded DV subset decoder (independent of ESPHome/radio).
// Takes a CRC-checked, *already decrypted* application payload. It DOES NOT
// perform AES, link parsing or IXML. Unsupported DIFs fail closed.
#include "xmq_semantic.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace xmq_runtime {

struct DecodedRecord {
  DvRecord meta;
  double value = 0;
  std::string unit;
  size_t offset = 0;
};

struct DvError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline std::string hex_byte(uint8_t v) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  std::string r(2, '0');
  r[0] = kHex[v >> 4];
  r[1] = kHex[v & 15];
  return r;
}

inline std::vector<uint8_t> parse_hex(const std::string &s) {
  if (s.size() % 2 || s.size() > 8192) throw DvError("invalid hex byte count");
  std::vector<uint8_t> out;
  out.reserve(s.size()/2);
  for (size_t i = 0; i < s.size(); i+=2) {
    unsigned v=0;
    if (!parse_uint(s.substr(i,2),16,v)) throw DvError("invalid hex digit");
    out.push_back(static_cast<uint8_t>(v));
  }
  return out;
}

inline bool supported_vif(uint8_t vif, std::string &range, std::string &unit,
                          double &scale) {
  const uint8_t raw = vif & 0x7F;
  if (raw >= 0x10 && raw <= 0x17) {
    range="Volume"; unit="m3";
    scale=std::pow(10.0,static_cast<int>(raw-0x10)-6);
    return true;
  }
  if (raw >= 0x38 && raw <= 0x3F) {
    range="VolumeFlow"; unit="m3h";
    scale=std::pow(10.0,static_cast<int>(raw-0x38)-6);
    return true;
  }
  if (raw == 0x6E) {
    range="HeatCostAllocation"; unit="hca"; scale=1;
    return true;
  }
  if (raw >= 0x64 && raw <= 0x67) {
    range="ExternalTemperature"; unit="c";
    scale=std::pow(10.0,static_cast<int>(raw-0x64)-3);
    return true;
  }
  if (raw >= 0x5C && raw <= 0x5F) {
    range="ReturnTemperature"; unit="c";
    scale=std::pow(10.0,static_cast<int>(raw-0x5C)-3);
    return true;
  }
  return false;
}

inline std::vector<DecodedRecord> decode_dv_subset(
    const std::vector<uint8_t> &payload, size_t max_records=64) {
  if (payload.size() > 1024) throw DvError("payload size limit");
  std::vector<DecodedRecord> records;
  size_t p=0;
  while (p < payload.size()) {
    if (payload[p] == 0x2F) { ++p; continue; }
    if (records.size() >= max_records) throw DvError("record count limit");
    const size_t start=p;
    const uint8_t dif=payload[p++];
    const uint8_t coding=dif & 0x0F;
    if (coding == 0x0F) break; // manufacturer-specific tail
    // DIF encoding 0x00 is a no-data entry with a VIF.
    unsigned storage=(dif>>6)&1;
    unsigned tariff=0, subunit=0;
    unsigned shift=1;
    std::string key=hex_byte(dif);
    uint8_t ext=dif;
    for (unsigned n=0; ext&0x80; ++n) {
      if (n>=10 || p>=payload.size()) throw DvError("truncated DIFE");
      ext=payload[p++];
      key+=hex_byte(ext);
      if (shift>=32) throw DvError("storage index too large");
      storage|=(static_cast<unsigned>(ext & 0x0F)<<shift);
      shift+=4;
      tariff|=(static_cast<unsigned>((ext>>4)&0x03)<<(n*2));
      subunit|=(static_cast<unsigned>((ext>>6)&1)<<n);
    }
    if (p>=payload.size()) throw DvError("missing VIF");
    uint8_t vif=payload[p++];
    key+=hex_byte(vif);
    // Extended VIF keys are preserved but not yet evaluated.
    std::string range, unit;
    double scale=1;
    bool simple_vif=!(vif&0x80) && supported_vif(vif,range,unit,scale);
    if ((vif&0x7f)==0x7b || (vif&0x7f)==0x7d) {
      if (p>=payload.size()) throw DvError("missing VIF extension");
      const uint8_t extension=payload[p++];
      key+=hex_byte(extension);
      if ((vif&0x7f)==0x7d && extension==0x3A) {
        range="Dimensionless"; unit="counter"; scale=1;
        simple_vif=true;
      }
      if (extension&0x80) throw DvError("VIF extension chain not supported");
    } else {
      unsigned n=0;
      while (vif&0x80) {
        if (++n>10 || p>=payload.size()) throw DvError("truncated VIFE");
        vif=payload[p++];
        key+=hex_byte(vif);
      }
    }
    size_t len=0;
    bool bcd=false;
    switch (coding) {
      case 0: len=0; break;
      case 1: len=1; break;
      case 2: len=2; break;
      case 3: len=3; break;
      case 4: len=4; break;
      case 6: len=6; break;
      case 7: len=8; break;
      case 9: len=1; bcd=true; break;
      case 10: len=2; bcd=true; break;
      case 11: len=3; bcd=true; break;
      case 12: len=4; bcd=true; break;
      default: throw DvError("unsupported DIF coding");
    }
    if (len>payload.size()-p) throw DvError("truncated DV value");
    uint64_t bits=0;
    double numeric=0;
    if (!bcd) {
      for (size_t i=0;i<len;i++) bits|=static_cast<uint64_t>(payload[p+i]) << (8*i);
      if (len && (payload[p+len-1]&0x80)) {
        const unsigned width=static_cast<unsigned>(len*8);
        if (width<64) bits|=(~uint64_t(0))<<width;
        numeric=static_cast<double>(static_cast<int64_t>(bits));
      } else {
        numeric=static_cast<double>(bits);
      }
    } else {
      double mult=1;
      for (size_t i=0;i<len;i++) {
        const unsigned lo=payload[p+i]&15, hi=payload[p+i]>>4;
        if (lo>9 || hi>9) throw DvError("invalid BCD nibble");
        numeric+=lo*mult;mult*=10;
        numeric+=hi*mult;mult*=10;
      }
    }
    p+=len;
    if (simple_vif && len) {
      DecodedRecord rec;
      rec.meta={key,"Instantaneous",range,storage,tariff,subunit};
      // DIF bits 4/5 are measurement type. No-deviation is instantaneous.
      const unsigned mt=(dif>>4)&3;
      rec.meta.measurement_type=mt==0?"Instantaneous":
                                mt==1?"Maximum":mt==2?"Minimum":"AtError";
      rec.value=numeric*scale;
      rec.unit=unit;
      rec.offset=start;
      if (!std::isfinite(rec.value)) throw DvError("nonfinite scaled value");
      records.push_back(std::move(rec));
    }
  }
  return records;
}

} // namespace xmq_runtime
