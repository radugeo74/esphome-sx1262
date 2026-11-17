#include "xmq_telegram_capture.h"
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
