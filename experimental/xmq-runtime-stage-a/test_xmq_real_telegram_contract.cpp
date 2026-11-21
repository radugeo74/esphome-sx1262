#include "xmq_telegram_capture.h"
#include "xmq_ci_route_contract.h"
#include "xmq_ci_security_census.h"
// This is the ACTUAL existing production Telegram declaration, not a mock.
// The unchanged upstream Telegram declares a private constructor with an
// intentionally unused argument; contain its warning, keep -Werror for tests.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include "../../components/wmbus_common/wmbus.h"
#pragma GCC diagnostic pop
#include <iostream>
#include <stdexcept>
#include <type_traits>

// Link-only fixture for a harmless upstream header-global NoDifVifKey("").
// The actual legacy parser is NOT exercised by this header contract test.
// Refuse any real DIF/VIF input instead of pretending to implement extractDV.
void extractDV(std::string &key, uchar *dif, int *vif,
               bool *has_difes, bool *has_vifes) {
  if (!key.empty()) throw std::runtime_error("unexpected DV parser input");
  if (dif) *dif=0;
  if (vif) *vif=0;
  if (has_difes) *has_difes=false;
  if (has_vifes) *has_vifes=false;
}

static void require(bool yes, const char *reason) {
  if (!yes) throw std::runtime_error(reason);
}

int main() {
  using namespace xmq_runtime;
  try {
    static_assert(std::is_same<decltype(Telegram::decryption_failed),bool>::value,
                  "real Telegram decryption flag API changed");
    static_assert(std::is_same<decltype(Telegram::frame),std::vector<uchar>>::value,
                  "real Telegram decoded frame API changed");
    ::Telegram original;
    original.decryption_failed=false;
    original.discard=false;
    original.header_size=3;
    original.suffix_size=1;
    original.tpl_ci=0xA2;
    original.frame={0x11,0x22,0x33,0x0F,0x9F,0x29,0x2D,0xEE};
    ::Address converter;
    converter.mfct=0x4CAE;
    converter.version=0x38;
    converter.type=0x37;
    ::Address inner;
    inner.mfct=0x5068;
    inner.version=0x94;
    inner.type=0x80;
    original.addresses.push_back(converter);
    original.addresses.push_back(inner);

    VerifiedApplication snapshot;
    require(capture_verified_payload(original,true,
            ApplicationEncoding::OriginalIxml,snapshot),
            "actual Telegram type capture failed");
    require(snapshot.inner_mvt.manufacturer=="TCH" &&
            snapshot.inner_mvt.version==0x94 &&
            snapshot.inner_mvt.type==0x80,
            "real Telegram inner address must win over converter address");
    require(snapshot.payload==parse_hex("0F9F292D"),
            "real Telegram header/suffix bounds changed");
    // Classify transport using the ACTUAL upstream Telegram::tpl_ci
    // member, not a guessed CI based on selected XMQ driver name.
    require(classify_xmq_telegram_route(
                static_cast<std::uint8_t>(original.tpl_ci), snapshot.encoding,
                true, true, false) ==
                XmqTelegramRoute::LegacyVerifiedProprietary,
            "real Telegram proprietary CI did not classify correctly");
    require(classify_xmq_telegram_route(
                static_cast<std::uint8_t>(original.tpl_ci), snapshot.encoding,
                false, true, true) == XmqTelegramRoute::Reject,
            "real proprietary CI could bypass verified legacy parser");

    // Only a declared standard CI may enter the standard DV path.
    ::Telegram standard;
    standard.decryption_failed = false;
    standard.discard = false;
    standard.header_size = original.header_size;
    standard.suffix_size = original.suffix_size;
    standard.frame = original.frame;
    standard.addresses = original.addresses;
    standard.tpl_ci = 0x7A;
    VerifiedApplication standard_snapshot;
    require(capture_verified_payload(
                standard, true, ApplicationEncoding::StandardDv,
                standard_snapshot),
            "real Telegram standard CI capture failed");
    require(classify_xmq_telegram_route(
                static_cast<std::uint8_t>(standard.tpl_ci),
                standard_snapshot.encoding, true, true, false) ==
                XmqTelegramRoute::LegacyVerifiedStandard,
            "real Telegram standard CI route failed");
    require(!capture_verified_payload(
                standard, true, ApplicationEncoding::OriginalIxml,
                standard_snapshot) && standard_snapshot.payload.empty(),
            "real Telegram standard CI was misclassified as proprietary");
    require(classify_xmq_telegram_route(
                static_cast<std::uint8_t>(standard.tpl_ci),
                ApplicationEncoding::StandardDv, false, true, true) ==
                XmqTelegramRoute::DirectTpl5StandardCandidate,
            "real Telegram standard CI candidate missing");
    require(classify_xmq_telegram_route(
                static_cast<std::uint8_t>(standard.tpl_ci),
                ApplicationEncoding::StandardDv, false, false, true) ==
                XmqTelegramRoute::Reject,
            "real Telegram standard CI bypassed CRC evidence");

    ::Telegram unknown;
    unknown.decryption_failed = false;
    unknown.discard = false;
    unknown.header_size = original.header_size;
    unknown.suffix_size = original.suffix_size;
    unknown.frame = original.frame;
    unknown.addresses = original.addresses;
    unknown.tpl_ci = 0x8C;
    VerifiedApplication denied;
    require(!capture_verified_payload(
                unknown, true, ApplicationEncoding::OriginalIxml,
                denied) && denied.payload.empty() &&
            classify_xmq_telegram_route(
                static_cast<std::uint8_t>(unknown.tpl_ci),
                ApplicationEncoding::OriginalIxml, true, true, false) ==
                XmqTelegramRoute::Reject,
            "unrecognized real Telegram CI reached XMQ");
    // Aggregate real upstream Telegram metadata without retaining any
    // M/A identity, ACC, raw data, decrypted values or meter secrets.
    CiSecurityCensus census;
    original.tpl_sec_mode = TPLSecurityMode::NoSecurity;
    require(census.observe_telegram(original, true) &&
            census.accepted[1][0] == 1,
            "real Telegram proprietary CI/security census failed");
    standard.tpl_sec_mode = TPLSecurityMode::AES_CBC_IV;
    require(census.observe_telegram(standard, true) &&
            census.accepted[3][1] == 1,
            "real Telegram standard CI mode-5 census failed");
    standard.tpl_sec_mode = TPLSecurityMode::AES_CBC_NO_IV;
    require(census.observe_telegram(standard, true) &&
            census.accepted[3][2] == 1,
            "real Telegram mode-7 census failed");
    require(!census.observe_telegram(unknown, false) &&
            census.rejected == 1,
            "unsuccessful legacy parser observed as valid CI evidence");
    require(!census.observe(0x72, 5, true, true, false) &&
            census.rejected == 2,
            "decryption-failed telegram entered metadata census");
    const auto preserved=original.frame;
    require(!capture_verified_payload(original,false,
            ApplicationEncoding::OriginalIxml,snapshot) &&
            snapshot.payload.empty(),
            "ID match must not override failed real meter handling");
    original.decryption_failed=true;
    require(!capture_verified_payload(original,true,
            ApplicationEncoding::OriginalIxml,snapshot) &&
            snapshot.payload.empty(),
            "decryption failed leaked real Telegram payload");
    require(original.frame==preserved,
            "shadow capture modified legacy Telegram frame");
    std::cout<<"REAL_TELEGRAM_CONTRACT_OK verified real wmbus_common declaration\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr<<"FAIL "<<error.what()<<"\n";
    return 1;
  }
}
