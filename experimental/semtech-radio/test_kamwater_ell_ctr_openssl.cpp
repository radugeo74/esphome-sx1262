#include "host_only/kamwater_ell_ctr_openssl.h"
#include "host_only/kamwater_verified_dll_ingress.h"
#include "host_only/kamwater_secure_ingress.h"
#include "components/wmbus_radio/pure_complete_frame.h"
#include "components/wmbus_native_sx126x/kamwater_tpl79_probe.h"
#include "components/wmbus_native_sx126x/kamwater_clear_records.h"
#include <cmath>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <vector>
int main() {
  using namespace xmq_portable::kamwater::ell_ctr;
  // Independent NIST AES-128 ECB known-answer check for the host backend.
  const uint8_t nist_key[16]={
    0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
    0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
  };
  const uint8_t nist_plain[16]={
    0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77,
    0x88,0x99,0xaa,0xbb,0xcc,0xdd,0xee,0xff
  };
  const uint8_t nist_cipher[16]={
    0x69,0xc4,0xe0,0xd8,0x6a,0x7b,0x04,0x30,
    0xd8,0xcd,0xb7,0x80,0x70,0xb4,0xc5,0x5a
  };
  uint8_t block[16]{};
  assert(aes_ecb_block(nist_key,nist_plain,block));
  for(int i=0;i<16;++i) assert(block[i]==nist_cipher[i]);
  uint8_t counter[16]{};
  counter[15]=0xff;increment_counter(counter);
  assert(counter[14]==1&&counter[15]==0);
  // Fully synthetic ELL II + public compact application, no meter key.
  const uint8_t m[2]={0x2D,0x2C};
  const uint8_t a[6]={0x78,0x56,0x34,0x12,0x01,0x07};
  const uint8_t tpl[17]={
    0x79,0xED,0xA8,0x6A,0xB6,0x71,0x00,0x08,0x19,
    0x00,0x00,0x08,0x19,0x00,0x00,0x7F,0x13
  };
  std::vector<uint8_t> clear(2+sizeof(tpl));
  for(size_t i=0;i<sizeof(tpl);++i) clear[2+i]=tpl[i];
  const uint16_t crc=xmq_portable::kamwater::signature::crc16_en13757(
      tpl,sizeof(tpl));
  clear[0]=static_cast<uint8_t>(crc);
  clear[1]=static_cast<uint8_t>(crc>>8);
  std::vector<uint8_t> encrypted={0x8D,0x32,0x01,0x03,0,0,0x20};
  Iv iv{};
  assert(derive_iv(m,a,encrypted.data(),encrypted.size(),iv)==Status::Ok);
  for(size_t offset=0;offset<clear.size();offset+=16) {
    uint8_t stream[16]{};
    assert(aes_ecb_block(nist_key,iv.bytes,stream));
    const size_t n=clear.size()-offset<16?clear.size()-offset:16;
    for(size_t j=0;j<n;++j) encrypted.push_back(clear[offset+j]^stream[j]);
    increment_counter(iv.bytes);
  }
  std::vector<uint8_t> decoded{};
  assert(decrypt_ell_ii(m,a,nist_key,encrypted.data(),encrypted.size(),decoded)
         ==DecryptStatus::Ok);
  assert(decoded.size()==sizeof(tpl));
  for(size_t i=0;i<sizeof(tpl);++i)assert(decoded[i]==tpl[i]);
  // Host-only composition: decrypted TPL79 signature + expanded-record
  // CRC must pass BEFORE the DIF/VIF parser produces a reading.
  uint8_t expanded[128]{};
  xmq_portable::kamwater::tpl79::Reading compact_result{};
  assert(xmq_portable::kamwater::tpl79::verify(
      decoded.data(),decoded.size(),expanded,sizeof(expanded),compact_result)
      ==xmq_portable::kamwater::tpl79::Status::Ok);
  assert(compact_result.profile_index==0);
  xmq_portable::kamwater::Reading reading{};
  assert(xmq_portable::kamwater::decode_clear_records(
      expanded,compact_result.expanded_size,reading)
      ==xmq_portable::kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-6.408)<1e-8);
  assert(reading.has_target && std::fabs(reading.target_m3-6.408)<1e-8);
  assert(reading.has_status && reading.status_bits==0x71);
  // A valid ELL CRC is insufficient: tampering with TPL data CRC is
  // detected independently even after recomputing the outer ELL CRC.
  auto altered_tpl=decoded;
  altered_tpl[3]^=1;
  const uint16_t new_ell_crc=xmq_portable::kamwater::signature::crc16_en13757(
      altered_tpl.data(),altered_tpl.size());
  auto altered_clear=std::vector<uint8_t>(2+altered_tpl.size());
  altered_clear[0]=static_cast<uint8_t>(new_ell_crc);
  altered_clear[1]=static_cast<uint8_t>(new_ell_crc>>8);
  for(size_t i=0;i<altered_tpl.size();++i) altered_clear[i+2]=altered_tpl[i];
  auto altered_encrypted=std::vector<uint8_t>(encrypted.begin(),encrypted.begin()+7);
  assert(derive_iv(m,a,altered_encrypted.data(),altered_encrypted.size(),iv)
         ==Status::Ok);
  for(size_t offset=0;offset<altered_clear.size();offset+=16) {
    uint8_t stream[16]{};
    assert(aes_ecb_block(nist_key,iv.bytes,stream));
    const size_t n=altered_clear.size()-offset<16?altered_clear.size()-offset:16;
    for(size_t j=0;j<n;++j)
      altered_encrypted.push_back(altered_clear[offset+j]^stream[j]);
    increment_counter(iv.bytes);
  }
  std::vector<uint8_t> altered_decoded{};
  assert(decrypt_ell_ii(m,a,nist_key,altered_encrypted.data(),
                        altered_encrypted.size(),altered_decoded)
         ==DecryptStatus::Ok);
  assert(xmq_portable::kamwater::tpl79::verify(
      altered_decoded.data(),altered_decoded.size(),
      expanded,sizeof(expanded),compact_result)
      ==xmq_portable::kamwater::tpl79::Status::DataCrcMismatch);
  assert(compact_result.profile_index==-1 && compact_result.expanded_size==0);
  // The precondition for a host-only encrypted application is a full
  // verified DLL header, not independently trusted supplied IV address.
  // Build a SYNTHETIC, short format-B DLL envelope around the ciphertext.
  std::vector<uint8_t> dll={
    0,0x44,0x2D,0x2C,0x78,0x56,0x34,0x12,0x01,0x07
  };
  dll.insert(dll.end(),encrypted.begin(),encrypted.end());
  dll[0]=static_cast<uint8_t>(dll.size()+1);
  const uint16_t dll_crc=wmbus_pure::crc16_en13757(dll.data(),dll.size());
  dll.push_back(static_cast<uint8_t>(dll_crc>>8));
  dll.push_back(static_cast<uint8_t>(dll_crc));
  wmbus_pure::CompleteFrame verified{};
  assert(wmbus_pure::verify_complete_frame(
      dll.data(),dll.size(),wmbus_pure::CrcFrameFormat::B,verified)
      ==wmbus_pure::CompleteStatus::Ok);
  assert(verified.header.manufacturer==0x2C2D);
  assert(verified.header.meter_id==12345678u);
  assert(verified.header.version==0x01 && verified.header.device_type==0x07);
  assert(verified.header.ci==0x8D);
  using xmq_portable::kamwater::ingress::ExpectedMeter;
  using xmq_portable::kamwater::ingress::matches_expected;
  const ExpectedMeter expected_meter{0x2C2D,12345678u,0x01,0x07};
  assert(matches_expected(verified.header,expected_meter));
  // Both ELL AES-CTR/TPL79 and OMS CCM/TPL7A now share this host-only
  // verified-DLL, exact-identity, decode-and-publish-nothing ingress.
  xmq_portable::kamwater::secure_ingress::Output secure{};
  using SecureStatus=xmq_portable::kamwater::secure_ingress::Status;
  assert(xmq_portable::kamwater::secure_ingress::decode(
      dll.data(),dll.size(),wmbus_pure::CrcFrameFormat::B,
      expected_meter,nist_key,secure)==SecureStatus::Ok);
  assert(secure.profile_index==0);
  assert(std::fabs(secure.reading.total_m3-6.408)<1e-8);
  assert(secure.reading.has_target);
  // The exact identity check is required BEFORE IV construction.
  // Derive the IV address and ELL data ONLY from CRC-verified DLL bytes.
  std::vector<uint8_t> from_dll{};
  assert(decrypt_ell_ii(
      verified.bytes.data()+2,verified.bytes.data()+4,nist_key,
      verified.bytes.data()+10,verified.bytes.size()-10,from_dll)
      ==DecryptStatus::Ok);
  assert(from_dll==std::vector<uint8_t>(tpl,tpl+sizeof(tpl)));
  assert(xmq_portable::kamwater::tpl79::verify(
      from_dll.data(),from_dll.size(),expanded,sizeof(expanded),
      compact_result)==xmq_portable::kamwater::tpl79::Status::Ok);
  assert(xmq_portable::kamwater::decode_clear_records(
      expanded,compact_result.expanded_size,reading)
      ==xmq_portable::kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-6.408)<1e-8);
  // DLL corruption is rejected BEFORE decrypting or interpreting values.
  auto tampered_dll=dll;
  tampered_dll[5]^=1;
  assert(wmbus_pure::verify_complete_frame(
      tampered_dll.data(),tampered_dll.size(),
      wmbus_pure::CrcFrameFormat::B,verified)
      ==wmbus_pure::CompleteStatus::BadCrc);
  assert(verified.bytes.empty());
  // A valid re-CRC'd DLL is still not the configured meter identity.
  tampered_dll=dll;
  tampered_dll[4]=0x79;
  const uint16_t recalc=wmbus_pure::crc16_en13757(
      tampered_dll.data(),tampered_dll.size()-2);
  tampered_dll[tampered_dll.size()-2]=static_cast<uint8_t>(recalc>>8);
  tampered_dll.back()=static_cast<uint8_t>(recalc);
  assert(wmbus_pure::verify_complete_frame(
      tampered_dll.data(),tampered_dll.size(),
      wmbus_pure::CrcFrameFormat::B,verified)
      ==wmbus_pure::CompleteStatus::Ok);
  assert(verified.header.meter_id!=12345678u);
  assert(!matches_expected(verified.header,expected_meter));
  // Independently vary each configured field while keeping a valid DLL.
  // All four checks reject before reaching ELL processing.
  for(const size_t pos : {size_t(2),size_t(4),size_t(8),size_t(9)}) {
    tampered_dll=dll;
    tampered_dll[pos]^=1;
    const uint16_t renewed=wmbus_pure::crc16_en13757(
        tampered_dll.data(),tampered_dll.size()-2);
    tampered_dll[tampered_dll.size()-2]=static_cast<uint8_t>(renewed>>8);
    tampered_dll.back()=static_cast<uint8_t>(renewed);
    assert(wmbus_pure::verify_complete_frame(
        tampered_dll.data(),tampered_dll.size(),
        wmbus_pure::CrcFrameFormat::B,verified)
        ==wmbus_pure::CompleteStatus::Ok);
    assert(!matches_expected(verified.header,expected_meter));
  }
  assert(!matches_expected(verified.header,ExpectedMeter{}));
  // Identity mismatch must be denied by the caller before ELL processing.
  // Wrong key, corrupted payload, wrong mode and truncated ciphertext
  // must not publish any decrypted output.
  auto key_wrong=std::vector<uint8_t>(nist_key,nist_key+16);
  key_wrong[0]^=1;
  assert(decrypt_ell_ii(m,a,key_wrong.data(),encrypted.data(),
                       encrypted.size(),decoded)==DecryptStatus::BadCrc);
  assert(decoded.empty());
  auto bad=encrypted;
  bad.back()^=1;
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::BadCrc);
  assert(decoded.empty());
  bad=encrypted;bad[6]=0;
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::Unsupported);
  assert(decoded.empty());
  assert(decrypt_ell_ii(m,a,nist_key,encrypted.data(),8,decoded)
         ==DecryptStatus::Invalid);
  assert(decoded.empty());
  // Regression: every shortened ciphertext must be rejected without
  // returning any previously decoded measurements.
  // The two-byte recovered ELL CRC covers the entire recovered TPL.
  for (size_t lost : {size_t(1),size_t(2),size_t(8)}) {
    bad=encrypted;
    bad.resize(bad.size()-lost);
    decoded.assign(tpl,tpl+sizeof(tpl)); // deliberate stale output
    assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
           ==DecryptStatus::BadCrc);
    assert(decoded.empty());
  }
  // Unsupported ELL security modes, incorrect CI, and changed session
  // number are not valid AES-CTR frames from the configured meter.
  for (const uint8_t mode : {uint8_t(0x00),uint8_t(0x40),
                             uint8_t(0x60),uint8_t(0xE0)}) {
    bad=encrypted;
    bad[6]=mode;
    decoded.assign(tpl,tpl+sizeof(tpl));
    assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
           ==DecryptStatus::Unsupported);
    assert(decoded.empty());
  }
  bad=encrypted;bad[0]=0x8F;
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::Unsupported);
  assert(decoded.empty());
  bad=encrypted;bad[3]^=1; // ELL SN is part of the IV
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::BadCrc);
  assert(decoded.empty());
  // A missing key or missing frame pointer must never return stale data.
  decoded.assign(tpl,tpl+sizeof(tpl));
  assert(decrypt_ell_ii(m,a,nullptr,encrypted.data(),encrypted.size(),decoded)
         ==DecryptStatus::Invalid);
  assert(decoded.empty());
  decoded.assign(tpl,tpl+sizeof(tpl));
  assert(decrypt_ell_ii(m,a,nist_key,nullptr,encrypted.size(),decoded)
         ==DecryptStatus::Invalid);
  assert(decoded.empty());
  // Changed hop/repeated flags preserve the IV and valid decryption.
  bad=encrypted;bad[1]=0x20;
  assert(decrypt_ell_ii(m,a,nist_key,bad.data(),bad.size(),decoded)
         ==DecryptStatus::Ok);
  assert(decoded.size()==sizeof(tpl));
  return 0;
}
