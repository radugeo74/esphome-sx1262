// Host-only executable for comparing existing clear-application decoders
// to the POST-DECRYPTED portions of original upstream XMQ test fixtures.
// Does not accept/reinterpret a raw encrypted RF frame; no sensor publishing.
#include "components/wmbus_native_sx126x/minomess_clear_prefix.h"
#include "host_only/minomess_wired_frame.h"
#include "components/wmbus_native_sx126x/kamwater_clear_records.h"
#include "components/wmbus_native_sx126x/kamwater_tpl79_probe.h"
#include "host_only/apator162_ixml_fields.h"
#include "host_only/kamwater_status_semantics.h"
#include "host_only/kamwater_tpl_status_semantics.h"
#include "host_only/kamwater_numeric_semantics.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
#include <cmath>
static int hex_digit(char c) {
  if(c>='0' && c<='9') return c-'0';
  if(c>='a' && c<='f') return 10+c-'a';
  if(c>='A' && c<='F') return 10+c-'A';
  return -1;
}
int main(int argc,char **argv) {
  if(argc!=3 && argc!=4) return 2;
  if(argc==4 && std::strcmp(argv[1],"kamwater_compact_ref")!=0 &&
     std::strcmp(argv[1],"kamwater_numeric_compact_ref")!=0 &&
     std::strcmp(argv[1],"kamwater_compact_status_ref")!=0 &&
     std::strcmp(argv[1],"minomess_wired_configured")!=0)
    return 2;
  const char *src=argv[2];
  const size_t n=std::strlen(src);
  if(n==0 || n%2 || n>1024) return 2;
  std::vector<uint8_t> bytes;
  for(size_t i=0;i<n;i+=2) {
    const int hi=hex_digit(src[i]),lo=hex_digit(src[i+1]);
    if(hi<0 || lo<0) return 2;
    bytes.push_back(static_cast<uint8_t>((hi<<4)|lo));
  }
  const bool numeric_plain=std::strcmp(argv[1],"kamwater_numeric")==0;
  const bool numeric_compact=std::strcmp(argv[1],"kamwater_numeric_compact")==0;
  const bool numeric_ref=std::strcmp(argv[1],"kamwater_numeric_compact_ref")==0;
  if(numeric_plain || numeric_compact || numeric_ref) {
    std::vector<uint8_t> application=bytes;
    uint8_t expanded[160]{};
    xmq_portable::kamwater::tpl79::Reading compact{};
    if(numeric_compact || numeric_ref) {
      xmq_portable::kamwater::tpl79::Status verified{};
      if(numeric_ref) {
        if(argc!=4) return 2;
        const char *ref=argv[3];
        const size_t count=std::strlen(ref);
        if(!count || count%2 || count>1024) return 2;
        std::vector<uint8_t> previous;
        for(size_t i=0;i<count;i+=2) {
          const int hi=hex_digit(ref[i]),lo=hex_digit(ref[i+1]);
          if(hi<0 || lo<0) return 2;
          previous.push_back(static_cast<uint8_t>((hi<<4)|lo));
        }
        verified=xmq_portable::kamwater::tpl79::verify_with_full_reference(
            bytes.data(),bytes.size(),previous.data(),previous.size(),
            expanded,sizeof(expanded),compact);
      } else {
        if(argc!=3) return 2;
        verified=xmq_portable::kamwater::tpl79::verify(
            bytes.data(),bytes.size(),expanded,sizeof(expanded),compact);
      }
      if(verified!=xmq_portable::kamwater::tpl79::Status::Ok) return 3;
      application.assign(expanded,expanded+compact.expanded_size);
    } else if(argc!=3) return 2;
    xmq_portable::kamwater::numeric_semantics::Reading numeric{};
    if(xmq_portable::kamwater::numeric_semantics::decode(
         application.data(),application.size(),numeric)
       !=xmq_portable::kamwater::numeric_semantics::Status::Ok) return 3;
    for(const auto &field:numeric.fields)
      std::printf("%s=%.9f\n",field.name.c_str(),field.value);
    if(numeric.has_acoustic_noise)
      std::printf("accoustic_noise=%s\n",numeric.acoustic_noise.c_str());
    if(numeric.has_target_date)
      std::printf("target_date=%04d-%02d-%02d\n",
          numeric.target_date.year,numeric.target_date.month,
          numeric.target_date.day);
    return 0;
  }
  if(std::strcmp(argv[1],"minomess_wired")==0 ||
     std::strcmp(argv[1],"minomess_wired_configured")==0) {
    const bool configured=
        std::strcmp(argv[1],"minomess_wired_configured")==0;
    if(configured != (argc==4)) return 2;
    xmq_portable::minomess::wired::Reading reading{};
    const auto result=configured
        ? xmq_portable::minomess::wired::decode_configured(
              bytes.data(),bytes.size(),argv[3],reading)
        : xmq_portable::minomess::wired::decode(
              bytes.data(),bytes.size(),reading);
    if(result!=xmq_portable::minomess::wired::Status::Ok) return 3;
    const auto &m=reading.meter;
    std::printf("total_m3=%.9f\n",m.total_m3);
    if(m.has_target)
      std::printf("target_m3=%.9f\n",m.target_m3);
    else std::puts("target_m3=null");
    if(m.has_meter_date)
      std::printf("meter_date=%04d-%02d-%02d\n",
                  m.meter_date.year,m.meter_date.month,m.meter_date.day);
    if(m.has_target_date)
      std::printf("target_date=%04d-%02d-%02d\n",
                  m.target_date.year,m.target_date.month,m.target_date.day);
    if(reading.has_gateway_rssi)
      std::printf("rssi_dbm=%d\n",reading.gateway_rssi_dbm);
    if(reading.has_actuality_duration)
      std::printf("actuality_duration_s=%u\n",
                  static_cast<unsigned>(reading.actuality_duration_s));
    if(m.status_bits==0)
      std::puts("status=OK");
    return 0;
  }
  if(std::strcmp(argv[1],"minomess")==0) {
    xmq_portable::minomess::Reading reading{};
    const auto result=xmq_portable::minomess::decode_clear_application(
        bytes.data(),bytes.size(),reading);
    if(result!=xmq_portable::minomess::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    if(reading.has_meter_date)
      std::printf("meter_date=%04d-%02d-%02d\n",
          reading.meter_date.year,reading.meter_date.month,
          reading.meter_date.day);
    if(reading.has_target_date)
      std::printf("target_date=%04d-%02d-%02d\n",
          reading.target_date.year,reading.target_date.month,
          reading.target_date.day);
    // Only claim OK for the verified empty status bits; no unreviewed
    // bit-to-name translations for nonzero alarm masks.
    if(reading.status_bits==0)
      std::puts("status=OK");
    return 0;
  }
  if(std::strcmp(argv[1],"apator162")==0) {
    xmq_portable::apator162::Reading reading{};
    const auto result=xmq_portable::apator162::decode_clear_manufacturer(
        bytes.data(),bytes.size(),reading);
    if(result!=xmq_portable::apator162::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_compact_status_ref")==0) {
    // Reviewed source #7: data CRC describes independently supplied
    // preceding FULL application #6, not current compact measurements.
    // Reference provenance must be verified separately before live use.
    if(argc!=4) return 2;
    const char *ref=argv[3];
    const size_t count=std::strlen(ref);
    if(!count || count%2 || count>1024) return 2;
    std::vector<uint8_t> previous;
    for(size_t i=0;i<count;i+=2) {
      const int hi=hex_digit(ref[i]),lo=hex_digit(ref[i+1]);
      if(hi<0 || lo<0) return 2;
      previous.push_back(static_cast<uint8_t>((hi<<4)|lo));
    }
    uint8_t expanded[160]{};
    xmq_portable::kamwater::tpl79::Reading compact{};
    if(xmq_portable::kamwater::tpl79::verify_with_full_reference(
           bytes.data(),bytes.size(),previous.data(),previous.size(),
           expanded,sizeof(expanded),compact)
       !=xmq_portable::kamwater::tpl79::Status::Ok) return 3;
    xmq_portable::kamwater::status_semantics::Reading status{};
    if(xmq_portable::kamwater::status_semantics::decode(
           expanded,compact.expanded_size,status)
       !=xmq_portable::kamwater::status_semantics::Status::Ok ||
       !status.modern || status.hot) return 3;
    std::printf("status=%s\n",status.status.c_str());
    std::printf("time_dry=%s\n",status.time_dry.c_str());
    std::printf("time_reversed=%s\n",status.time_reversed.c_str());
    std::printf("time_leaking=%s\n",status.time_leaking.c_str());
    std::printf("time_bursting=%s\n",status.time_bursting.c_str());
    std::printf("time_flow_above_q4=%s\n",status.time_flow_above_q4.c_str());
    std::printf("time_low_ambient_temperature=%s\n",
                status.time_low_ambient_temperature.c_str());
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_compact_status")==0) {
    // Only an already-clear compact application with validated TPL79
    // format signature and reconstructed-data CRC may reach status parsing.
    // This is a host parity adapter, not a live RF transport/ELL gate.
    uint8_t expanded[160]{};
    xmq_portable::kamwater::tpl79::Reading compact{};
    if(xmq_portable::kamwater::tpl79::verify(
           bytes.data(),bytes.size(),expanded,sizeof(expanded),compact)
       !=xmq_portable::kamwater::tpl79::Status::Ok) return 3;
    xmq_portable::kamwater::status_semantics::Reading status{};
    if(xmq_portable::kamwater::status_semantics::decode(
           expanded,compact.expanded_size,status)
       !=xmq_portable::kamwater::status_semantics::Status::Ok) return 3;
    // The reviewed #2/#4/#8 source fixtures use the legacy status profile.
    if(status.modern || !status.has_current_status) return 3;
    std::printf("status=%s\n",status.status.c_str());
    std::printf("current_status=%s\n",status.current_status.c_str());
    std::printf("time_dry=%s\n",status.time_dry.c_str());
    std::printf("time_reversed=%s\n",status.time_reversed.c_str());
    std::printf("time_leaking=%s\n",status.time_leaking.c_str());
    std::printf("time_bursting=%s\n",status.time_bursting.c_str());
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_compact_ref")==0) {
    if(argc!=4) return 2;
    const char *ref_hex=argv[3];
    const size_t ref_len=std::strlen(ref_hex);
    if(!ref_len || ref_len%2 || ref_len>1024) return 2;
    std::vector<uint8_t> reference;
    for(size_t i=0;i<ref_len;i+=2) {
      const int hi=hex_digit(ref_hex[i]),lo=hex_digit(ref_hex[i+1]);
      if(hi<0 || lo<0) return 2;
      reference.push_back(static_cast<uint8_t>((hi<<4)|lo));
    }
    uint8_t expanded[160]{};
    xmq_portable::kamwater::tpl79::Reading compact{};
    const auto status=xmq_portable::kamwater::tpl79::verify_with_full_reference(
        bytes.data(),bytes.size(),reference.data(),reference.size(),
        expanded,sizeof(expanded),compact);
    if(status==xmq_portable::kamwater::tpl79::Status::DataCrcMismatch)
      return 4;
    if(status!=xmq_portable::kamwater::tpl79::Status::Ok) return 3;
    xmq_portable::kamwater::Reading reading{};
    if(xmq_portable::kamwater::decode_clear_records(
        expanded,compact.expanded_size,reading)
        !=xmq_portable::kamwater::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_compact")==0) {
    uint8_t expanded[160]{};
    xmq_portable::kamwater::tpl79::Reading compact{};
    const auto compact_status=xmq_portable::kamwater::tpl79::verify(
        bytes.data(),bytes.size(),expanded,sizeof(expanded),compact);
    // Separate a verified compact data-CRC mismatch (4) from other
    // parsing failures (3); never expose any readings on either.
    if(compact_status==xmq_portable::kamwater::tpl79::Status::DataCrcMismatch)
      return 4;
    if(compact_status!=xmq_portable::kamwater::tpl79::Status::Ok)
      return 3;
    xmq_portable::kamwater::Reading reading{};
    if(xmq_portable::kamwater::decode_clear_records(
        expanded,compact.expanded_size,reading)
        !=xmq_portable::kamwater::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_tpl_target")==0) {
    // Original #13: short unencrypted TPL 7A with generic error and
    // a storage-one target volume, without any current total record.
    // Reuse the shared standard DIF/VIF reader, not fake a total.
    xmq_portable::kamwater::tpl_status::Reading header{};
    if(xmq_portable::kamwater::tpl_status::decode_clear_short_tpl(
          bytes.data(),bytes.size(),header)
       !=xmq_portable::kamwater::tpl_status::Status::Ok ||
       header.status!="ERROR" || header.has_durations) return 3;
    xmq_portable::standard::Record records[48]{};
    size_t count=0;
    if(xmq_portable::standard::records(bytes.data()+5,bytes.size()-5,
                                      records,48,count)
       !=xmq_portable::standard::Status::Ok) return 3;
    bool found=false;
    double target=0;
    for(size_t i=0;i<count;++i) {
      const auto &rec=records[i];
      if(!rec.numeric || rec.function!=0 || rec.tariff!=0 ||
         rec.subunit!=0 || rec.vif_extension_count!=0 ||
         rec.vif<0x10 || rec.vif>0x17) continue;
      // Original #13 has no current total; do not infer one.
      if(rec.storage!=1 || found) return 3;
      if(!xmq_portable::standard::to_canonical(rec.vif,rec.value,target)
         || !std::isfinite(target)) return 3;
      found=true;
    }
    if(!found) return 3;
    std::printf("target_m3=%.9f\n",target);
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_tpl_status")==0) {
    xmq_portable::kamwater::tpl_status::Reading parsed{};
    if(xmq_portable::kamwater::tpl_status::decode_clear_short_tpl(
        bytes.data(),bytes.size(),parsed)
        !=xmq_portable::kamwater::tpl_status::Status::Ok) return 3;
    std::printf("status=%s\n",parsed.status.c_str());
    if(parsed.has_durations) {
      const auto &s=parsed.durations;
      std::printf("time_dry=%s\n",s.time_dry.c_str());
      std::printf("time_reversed=%s\n",s.time_reversed.c_str());
      std::printf("time_leaking=%s\n",s.time_leaking.c_str());
      std::printf("time_bursting=%s\n",s.time_bursting.c_str());
      if(s.modern) {
        std::printf("time_flow_above_q4=%s\n",
                    s.time_flow_above_q4.c_str());
        if(s.hot)
          std::printf("time_high_ambient_temperature=%s\n",
                      s.time_high_ambient_temperature.c_str());
        else
          std::printf("time_low_ambient_temperature=%s\n",
                      s.time_low_ambient_temperature.c_str());
      }
    }
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_status")==0) {
    xmq_portable::kamwater::status_semantics::Reading status{};
    if(xmq_portable::kamwater::status_semantics::decode(
         bytes.data(),bytes.size(),status)
         !=xmq_portable::kamwater::status_semantics::Status::Ok) return 3;
    std::printf("status=%s\n",status.status.c_str());
    std::printf("time_dry=%s\n",status.time_dry.c_str());
    std::printf("time_reversed=%s\n",status.time_reversed.c_str());
    std::printf("time_leaking=%s\n",status.time_leaking.c_str());
    std::printf("time_bursting=%s\n",status.time_bursting.c_str());
    if(status.has_current_status)
      std::printf("current_status=%s\n",status.current_status.c_str());
    if(status.modern) {
      std::printf("time_flow_above_q4=%s\n",
                  status.time_flow_above_q4.c_str());
      if(status.hot)
        std::printf("time_high_ambient_temperature=%s\n",
                    status.time_high_ambient_temperature.c_str());
      else
        std::printf("time_low_ambient_temperature=%s\n",
                    status.time_low_ambient_temperature.c_str());
    }
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater_prefixed")==0) {
    // Original kamwater.xmq #3 clear-application sample carries exactly
    // three leading bytes 46 88 78 before its DIF/VIF record stream.
    // Admit only this reviewed prefix in this host-only parity adapter;
    // do not relax the generic DIF/VIF parser or RF dispatch.
    if(bytes.size()<4 || bytes[0]!=0x46 ||
       bytes[1]!=0x88 || bytes[2]!=0x78) return 3;
    xmq_portable::kamwater::Reading reading{};
    if(xmq_portable::kamwater::decode_clear_records(
        bytes.data()+3,bytes.size()-3,reading)
        !=xmq_portable::kamwater::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    return 0;
  }
  if(std::strcmp(argv[1],"kamwater")==0) {
    xmq_portable::kamwater::Reading reading{};
    const auto result=xmq_portable::kamwater::decode_clear_records(
        bytes.data(),bytes.size(),reading);
    if(result!=xmq_portable::kamwater::Status::Ok) return 3;
    std::printf("total_m3=%.9f\n",reading.total_m3);
    if(reading.has_target)
      std::printf("target_m3=%.9f\n",reading.target_m3);
    else std::puts("target_m3=null");
    return 0;
  }
  return 2;
}
