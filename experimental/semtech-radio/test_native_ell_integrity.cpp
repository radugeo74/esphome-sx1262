#include "components/wmbus_native_sx126x/native_ell_integrity.h"
#include <cassert>
#include <cstdint>
#include <vector>
#include <string>
using namespace wmbus_pure::identity;
static std::vector<uint8_t> make_ell(uint8_t ci,uint8_t mode) {
  // Synthetic converter DLL. No user IDs, keys or real telegrams.
  std::vector<uint8_t> f{
    0,0x44,0x68,0x50,0x36,0x49,0x46,0x00,0x95,0x37,ci,
    0x00,0x01 // CC, ACC
  };
  if (ci==0x8E || ci==0x8F)
    f.insert(f.end(),{0x24,0x23,0x25,0x38,0x95,0x66,0xFE,0x06});
  if (ci==0x8D || ci==0x8F) {
    const uint32_t sn=static_cast<uint32_t>(mode)<<29;
    for (unsigned i=0;i<4;++i) f.push_back(static_cast<uint8_t>(sn>>(8*i)));
    f.insert(f.end(),{0,0}); // ELL payload CRC
  }
  const size_t payload=f.size();
  f.insert(f.end(),{0x02,0x13,0x34,0x12});
  if (ci==0x8D || ci==0x8F) {
    const uint16_t crc=wmbus_pure::crc16_en13757(f.data()+payload,f.size()-payload);
    f[payload-2]=static_cast<uint8_t>(crc);
    f[payload-1]=static_cast<uint8_t>(crc>>8);
  }
  f[0]=static_cast<uint8_t>(f.size()-1);
  return f;
}
int main() {
  for (uint8_t ci : {uint8_t{0x8D},uint8_t{0x8F}}) {
    auto frame=make_ell(ci,0);
    const auto parsed=inspect(frame.data(),frame.size());
    assert(parsed.status==Status::Ok && parsed.has_ell_session);
    auto check=inspect_ell_payload(frame.data(),frame.size(),parsed);
    assert(check.status==EllIntegrity::Valid);
    assert(check.expected==check.calculated);
    frame.back()^=1;
    check=inspect_ell_payload(frame.data(),frame.size(),
                              inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::BadCrc);
    frame.pop_back();
    frame[0]=static_cast<uint8_t>(frame.size()-1);
    check=inspect_ell_payload(frame.data(),frame.size(),
                              inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::BadCrc);
  }
  for (uint8_t ci : {uint8_t{0x8C},uint8_t{0x8E}}) {
    auto frame=make_ell(ci,0);
    auto check=inspect_ell_payload(frame.data(),frame.size(),
                                   inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::NoPayloadCrc);
  }
  for (uint8_t mode : {uint8_t{1},uint8_t{2},uint8_t{7}}) {
    auto frame=make_ell(0x8F,mode);
    auto check=inspect_ell_payload(frame.data(),frame.size(),
                                   inspect(frame.data(),frame.size()));
    assert(check.status==EllIntegrity::EncryptedOrUnknown);
  }
  // Verified plaintext ELL IV with an immediately nested TPL long header.
  auto nested=make_ell(0x8F,0);
  const uint8_t tpl[]={0x72,0x25,0x38,0x95,0x66,0x24,0x23,
                       0xFE,0x06,0x4E,0x00,0x00,0x00};
  const auto original=inspect(nested.data(),nested.size());
  assert(original.status==Status::Ok);
  nested.resize(original.application_offset);
  nested.insert(nested.end(),tpl,tpl+sizeof(tpl));
  nested[0]=static_cast<uint8_t>(nested.size()-1);
  const uint16_t nested_crc=wmbus_pure::crc16_en13757(
      nested.data()+original.application_offset,
      nested.size()-original.application_offset);
  nested[original.application_offset-2]=static_cast<uint8_t>(nested_crc);
  nested[original.application_offset-1]=static_cast<uint8_t>(nested_crc>>8);
  auto parsed_nested=inspect(nested.data(),nested.size());
  auto tp=inspect_nested_tpl(nested.data(),nested.size(),parsed_nested);
  assert(tp.status==NestedTplStatus::LongHeader && tp.has_address);
  assert(same_id(tp.address,"66953825") &&
         tp.address.manufacturer==0x2324);
  assert(tp.application_offset==nested.size());
  assert(tp.has_config_word && tp.tpl_security_mode==0);
  // ELL integrity may be valid while the nested TPL is encrypted.
  auto tpl_encrypted=nested;
  tpl_encrypted[original.application_offset+12]=0x05;
  const uint16_t tpl_encrypted_crc=wmbus_pure::crc16_en13757(
      tpl_encrypted.data()+original.application_offset,
      tpl_encrypted.size()-original.application_offset);
  tpl_encrypted[original.application_offset-2]=static_cast<uint8_t>(tpl_encrypted_crc);
  tpl_encrypted[original.application_offset-1]=static_cast<uint8_t>(tpl_encrypted_crc>>8);
  tp=inspect_nested_tpl(tpl_encrypted.data(),tpl_encrypted.size(),
                        inspect(tpl_encrypted.data(),tpl_encrypted.size()));
  assert(tp.status==NestedTplStatus::LongHeader && tp.has_address);
  assert(tp.has_config_word && tp.tpl_security_mode==5);
  // DLL converter and ELL address may differ from the nested TPL meter.
  // Only the target TPL ID is configured: diagnostic preference is TPL.
  auto target=identify_nested_candidate(nested.data(),nested.size(),
      inspect(nested.data(),nested.size()),
      std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Tpl);
  assert(target.configured_candidate && !target.multiple_configured_ids);
  assert(!target.permits_publication);
  // A configured DLL converter plus a distinct configured TPL target
  // must be reported as a conflict, never assigned a driver.
  target=identify_nested_candidate(nested.data(),nested.size(),
      inspect(nested.data(),nested.size()),
      std::vector<std::string>{"00464936","66953825"});
  assert(target.candidate==CandidateSource::Conflict);
  assert(target.multiple_configured_ids && !target.configured_candidate);
  assert(!target.permits_publication);
  // The same configured ID in ELL and TPL is not a conflict.
  target=identify_nested_candidate(nested.data(),nested.size(),
      inspect(nested.data(),nested.size()),
      std::vector<std::string>{"66953825","12345678"});
  assert(target.candidate==CandidateSource::Tpl && !target.multiple_configured_ids);
  // If only the converter ID is configured, nested TPL does not
  // inherit its binding.
  target=identify_nested_candidate(nested.data(),nested.size(),
      inspect(nested.data(),nested.size()),
      std::vector<std::string>{"00464936"});
  assert(target.candidate==CandidateSource::Tpl && !target.configured_candidate);
  // Even if the TPL security word indicates encryption, metadata
  // remains inspectable but cannot authorize publication.
  target=identify_nested_candidate(tpl_encrypted.data(),tpl_encrypted.size(),
      inspect(tpl_encrypted.data(),tpl_encrypted.size()),
      std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Tpl && !target.permits_publication);
  // Never trust a stale parsed header if a frame's stated length changes.
  auto mismatched_length=nested;
  mismatched_length[0]=static_cast<uint8_t>(mismatched_length[0]-1);
  const auto stale_identity=inspect(nested.data(),nested.size());
  assert(inspect_ell_payload(mismatched_length.data(),
          mismatched_length.size(),stale_identity).status==
          EllIntegrity::Unsupported);
  tp=inspect_nested_tpl(mismatched_length.data(),
                        mismatched_length.size(),stale_identity);
  assert(tp.status==NestedTplStatus::NotVerified && !tp.has_address);
  target=identify_nested_candidate(mismatched_length.data(),
       mismatched_length.size(),stale_identity,
       std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Unsupported &&
         !target.configured_candidate && !target.permits_publication);
  // A valid DLL CRC does not help if the ELL payload CRC is wrong.
  auto bad_nested=nested;
  bad_nested[original.application_offset-1]^=0x01;
  tp=inspect_nested_tpl(bad_nested.data(),bad_nested.size(),
                         inspect(bad_nested.data(),bad_nested.size()));
  assert(tp.status==NestedTplStatus::NotVerified && !tp.has_address &&
         !tp.has_config_word);
  target=identify_nested_candidate(bad_nested.data(),bad_nested.size(),
      inspect(bad_nested.data(),bad_nested.size()),
      std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Unsupported &&
         !target.configured_candidate && !target.permits_publication);
  // ELL encrypted/unknown sessions are never treated as plaintext.
  auto encrypted=make_ell(0x8F,1);
  encrypted.resize(original.application_offset);
  encrypted.insert(encrypted.end(),tpl,tpl+sizeof(tpl));
  encrypted[0]=static_cast<uint8_t>(encrypted.size()-1);
  tp=inspect_nested_tpl(encrypted.data(),encrypted.size(),
                        inspect(encrypted.data(),encrypted.size()));
  assert(tp.status==NestedTplStatus::NotVerified);
  // Short nested CI, unknown NWL/AFL and truncated header fail closed.
  auto short_tpl=nested;
  short_tpl.resize(original.application_offset);
  short_tpl.insert(short_tpl.end(),{0x7A,0,0,0,0});
  short_tpl[0]=static_cast<uint8_t>(short_tpl.size()-1);
  const uint16_t short_crc=wmbus_pure::crc16_en13757(
      short_tpl.data()+original.application_offset,5);
  short_tpl[original.application_offset-2]=static_cast<uint8_t>(short_crc);
  short_tpl[original.application_offset-1]=static_cast<uint8_t>(short_crc>>8);
  tp=inspect_nested_tpl(short_tpl.data(),short_tpl.size(),
                        inspect(short_tpl.data(),short_tpl.size()));
  assert(tp.status==NestedTplStatus::ShortHeader && !tp.has_address);
  assert(tp.has_config_word && tp.tpl_security_mode==0);
  auto short_encrypted=short_tpl;
  short_encrypted[original.application_offset+4]=0x04;
  const uint16_t short_encrypted_crc=wmbus_pure::crc16_en13757(
      short_encrypted.data()+original.application_offset,5);
  short_encrypted[original.application_offset-2]=static_cast<uint8_t>(short_encrypted_crc);
  short_encrypted[original.application_offset-1]=static_cast<uint8_t>(short_encrypted_crc>>8);
  tp=inspect_nested_tpl(short_encrypted.data(),short_encrypted.size(),
                        inspect(short_encrypted.data(),short_encrypted.size()));
  assert(tp.status==NestedTplStatus::ShortHeader && tp.has_config_word &&
         tp.tpl_security_mode==4);
  auto unknown_layer=short_tpl;
  unknown_layer[original.application_offset]=0x81;
  const uint16_t other_crc=wmbus_pure::crc16_en13757(
      unknown_layer.data()+original.application_offset,5);
  unknown_layer[original.application_offset-2]=static_cast<uint8_t>(other_crc);
  unknown_layer[original.application_offset-1]=static_cast<uint8_t>(other_crc>>8);
  tp=inspect_nested_tpl(unknown_layer.data(),unknown_layer.size(),
                        inspect(unknown_layer.data(),unknown_layer.size()));
  assert(tp.status==NestedTplStatus::NoTpl);
  target=identify_nested_candidate(unknown_layer.data(),unknown_layer.size(),
      inspect(unknown_layer.data(),unknown_layer.size()),
      std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Unsupported &&
         !target.configured_candidate && !target.permits_publication);
  auto truncated_tpl=nested;
  truncated_tpl.resize(original.application_offset+2);
  truncated_tpl[0]=static_cast<uint8_t>(truncated_tpl.size()-1);
  const uint16_t truncated_crc=wmbus_pure::crc16_en13757(
      truncated_tpl.data()+original.application_offset,2);
  truncated_tpl[original.application_offset-2]=static_cast<uint8_t>(truncated_crc);
  truncated_tpl[original.application_offset-1]=static_cast<uint8_t>(truncated_crc>>8);
  tp=inspect_nested_tpl(truncated_tpl.data(),truncated_tpl.size(),
                        inspect(truncated_tpl.data(),truncated_tpl.size()));
  assert(tp.status==NestedTplStatus::TooShort);
  target=identify_nested_candidate(truncated_tpl.data(),truncated_tpl.size(),
      inspect(truncated_tpl.data(),truncated_tpl.size()),
      std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Unsupported &&
         !target.configured_candidate && !target.permits_publication);
  // CRC-valid but malformed manufacturer in the nested long TPL.
  auto invalid_mfct=nested;
  invalid_mfct[original.application_offset+5]=0;
  invalid_mfct[original.application_offset+6]=0;
  const uint16_t mfct_crc=wmbus_pure::crc16_en13757(
      invalid_mfct.data()+original.application_offset,
      invalid_mfct.size()-original.application_offset);
  invalid_mfct[original.application_offset-2]=static_cast<uint8_t>(mfct_crc);
  invalid_mfct[original.application_offset-1]=static_cast<uint8_t>(mfct_crc>>8);
  tp=inspect_nested_tpl(invalid_mfct.data(),invalid_mfct.size(),
                        inspect(invalid_mfct.data(),invalid_mfct.size()));
  assert(tp.status==NestedTplStatus::InvalidManufacturer && !tp.has_address);
  target=identify_nested_candidate(invalid_mfct.data(),invalid_mfct.size(),
      inspect(invalid_mfct.data(),invalid_mfct.size()),
      std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Unsupported &&
         !target.configured_candidate && !target.permits_publication);
  // Valid short TPL carries no separate meter identity: ELL remains
  // an advisory address only and publication is always forbidden.
  target=identify_nested_candidate(short_tpl.data(),short_tpl.size(),
      inspect(short_tpl.data(),short_tpl.size()),
      std::vector<std::string>{"66953825"});
  assert(target.candidate==CandidateSource::Ell &&
         target.configured_candidate && !target.permits_publication);
  const auto good=make_ell(0x8D,0);
  auto parsed=inspect(good.data(),good.size());
  assert(inspect_ell_payload(nullptr,0,parsed).status==EllIntegrity::Unsupported);
  parsed.application_offset=good.size()+1;
  assert(inspect_ell_payload(good.data(),good.size(),parsed).status==
         EllIntegrity::Truncated);
  auto unsupported=make_ell(0x8C,0);
  unsupported[10]=0x86;
  const auto unknown=inspect(unsupported.data(),unsupported.size());
  assert(inspect_ell_payload(unsupported.data(),unsupported.size(),unknown).status==
         EllIntegrity::Unsupported);
  return 0;
}
