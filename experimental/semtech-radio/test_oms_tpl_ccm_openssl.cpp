#include "host_only/oms_tpl_ccm_openssl.h"
#include "components/wmbus_native_sx126x/kamwater_clear_records.h"
#include <cmath>
#include <cassert>
#include <cstdint>
#include <vector>
using xmq_portable::oms_ccm::Output;
using xmq_portable::oms_ccm::Status;
static std::vector<uint8_t> synthetic_tpl(const uint8_t key[16],
                                          const uint8_t m[2],
                                          const uint8_t id[4]) {
  // Synthetic mode 10, N=FF (no partial encryption), KDF=1,
  // 8-byte tag, no optional key-version, 4-byte counter.
  std::vector<uint8_t> tpl={
      0x7A,0x30,0x00,0xFF,0x2A,0x10,0x01,0x04,0x03,0x02,0x01
  };
  const uint8_t plaintext[]={
      0x04,0x13,0x08,0x19,0,0,0x02,0xFF,0x20,0x00,0x00
  };
  uint8_t kenc[16]{};
  assert(xmq_portable::oms_ccm::derive_kenc(key,tpl.data()+7,id,kenc));
  const uint8_t nonce[13]={
      m[0],m[1],id[0],id[1],id[2],id[3],
      0x3C,0x16,0,0x01,0x02,0x03,0x04
  };
  EVP_CIPHER_CTX *ctx=EVP_CIPHER_CTX_new();
  assert(ctx);
  assert(EVP_EncryptInit_ex(ctx,EVP_aes_128_ccm(),nullptr,nullptr,nullptr)==1);
  assert(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_CCM_SET_IVLEN,13,nullptr)==1);
  assert(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_CCM_SET_TAG,8,nullptr)==1);
  assert(EVP_EncryptInit_ex(ctx,nullptr,nullptr,kenc,nonce)==1);
  int written=0;
  assert(EVP_EncryptUpdate(ctx,nullptr,&written,nullptr,sizeof(plaintext))==1);
  assert(EVP_EncryptUpdate(ctx,nullptr,&written,tpl.data(),7)==1);
  uint8_t ciphertext[sizeof(plaintext)]{};
  assert(EVP_EncryptUpdate(ctx,ciphertext,&written,plaintext,
                           sizeof(plaintext))==1);
  assert(written==static_cast<int>(sizeof(plaintext)));
  int tail=0;
  assert(EVP_EncryptFinal_ex(ctx,ciphertext+written,&tail)==1);
  assert(tail==0);
  uint8_t tag[8]{};
  assert(EVP_CIPHER_CTX_ctrl(ctx,EVP_CTRL_CCM_GET_TAG,8,tag)==1);
  EVP_CIPHER_CTX_free(ctx);
  tpl.insert(tpl.end(),ciphertext,ciphertext+sizeof(ciphertext));
  tpl.insert(tpl.end(),tag,tag+8);
  return tpl;
}
int main() {
  const uint8_t m[2]={0x37,0x2C};
  const uint8_t id[4]={0x17,0x35,0x24,0x68};
  const uint8_t key[16]={
     0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,
     0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0e,0x0f
  };
  const auto good=synthetic_tpl(key,m,id);
  Output out{};
  const auto decode=[&](const std::vector<uint8_t> &tpl) {
    return xmq_portable::oms_ccm::decode_short_tpl(
        m,id,0x3C,0x16,key,tpl.data(),tpl.size(),out);
  };
  assert(decode(good)==Status::Ok);
  assert(out.counter==0x01020304u && out.tpl_status==0x00);
  assert(out.clear_application==std::vector<uint8_t>(
      {0x04,0x13,0x08,0x19,0,0,0x02,0xFF,0x20,0,0}));
  // TPL 0x7A exposes a clear DIF/VIF application after tag verification,
  // not another 0x78 TPL CI. The shared Kamwater record parser must run
  // only on this authenticated output.
  xmq_portable::kamwater::Reading reading{};
  assert(xmq_portable::kamwater::decode_clear_records(
      out.clear_application.data(),out.clear_application.size(),reading)
      ==xmq_portable::kamwater::Status::Ok);
  assert(std::fabs(reading.total_m3-6.408)<1e-8);
  assert(reading.has_status && reading.status_bits==0);
  auto bad=good;
  bad.back()^=1; // Invalid CCM authentication tag.
  assert(decode(bad)==Status::BadTag && out.clear_application.empty());
  bad=good;bad[11]^=1; // Changed encrypted content.
  assert(decode(bad)==Status::BadTag && out.clear_application.empty());
  bad=good;bad[2]^=1; // Changed additional authenticated data.
  assert(decode(bad)==Status::BadTag && out.clear_application.empty());
  bad=good;bad[7]^=1; // Changed message counter.
  assert(decode(bad)==Status::BadTag && out.clear_application.empty());
  bad=good;bad[0]=0x79;
  assert(decode(bad)==Status::UnsupportedCi && out.clear_application.empty());
  bad=good;bad[4]=0x20;
  assert(decode(bad)==Status::UnsupportedMode && out.clear_application.empty());
  bad=good;bad[3]=0x0A;
  assert(decode(bad)==Status::UnsupportedPartial && out.clear_application.empty());
  bad=good;bad[5]=0x00;
  assert(decode(bad)==Status::UnsupportedKdf && out.clear_application.empty());
  bad=good;bad.resize(11);
  assert(decode(bad)==Status::Invalid && out.clear_application.empty());
  assert(xmq_portable::oms_ccm::decode_short_tpl(
      m,id,0x3C,0x16,nullptr,good.data(),good.size(),out)
      ==Status::MissingKey && out.clear_application.empty());
  auto wrong_id=std::vector<uint8_t>(id,id+4);
  wrong_id[0]^=1;
  assert(xmq_portable::oms_ccm::decode_short_tpl(
      m,wrong_id.data(),0x3C,0x16,key,good.data(),good.size(),out)
      ==Status::BadTag && out.clear_application.empty());
  return 0;
}
