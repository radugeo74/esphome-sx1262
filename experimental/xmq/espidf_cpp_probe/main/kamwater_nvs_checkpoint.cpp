// Real ESP-IDF API calls, but intentionally NOT called by app_main.
// Compiled and linked on esp32s3 in CI; NO on-device run or flash is claimed.
#include "kamwater_nvs_checkpoint.h"
#include <array>
#include <cstdio>
#include <cstring>
#include "nvs.h"

namespace xmq_portable { namespace kamwater { namespace nvs_probe {
namespace {
Result error(Status s,esp_err_t e=ESP_OK,
             checkpoint::Status c=checkpoint::Status::NotReady) {
  return Result{s,e,c};
}
} // namespace

bool make_key(const Meter &configured,char (&out)[16]) {
  std::memset(out,0,sizeof(out));
  if(configured.id>99999999u) return false;
  const int used=std::snprintf(out,sizeof(out),"kw%04X%08u",
      static_cast<unsigned>(configured.manufacturer),
      static_cast<unsigned>(configured.id));
  return used==14;
}

Result load(Session &session,const Meter &configured,uint32_t epoch) {
  session=Session{};
  char key[16]{};
  if(epoch==0 || !make_key(configured,key))
    return error(Status::InvalidArgument);
  nvs_handle_t handle{};
  const esp_err_t opened=nvs_open(kNamespace,NVS_READONLY,&handle);
  if(opened==ESP_ERR_NVS_NOT_FOUND) return error(Status::Missing,opened);
  if(opened!=ESP_OK) return error(Status::OpenFailed,opened);
  checkpoint::Bytes bytes{};
  size_t actual=bytes.size();
  const esp_err_t read=nvs_get_blob(handle,key,bytes.data(),&actual);
  nvs_close(handle);
  if(read==ESP_ERR_NVS_NOT_FOUND) return error(Status::Missing,read);
  if(read!=ESP_OK) return error(Status::ReadFailed,read);
  const auto validation=checkpoint::restore(session,bytes.data(),actual,
                                            configured,epoch);
  if(validation!=checkpoint::Status::Ok) {
    session=Session{};
    return error(Status::CheckpointRejected,ESP_OK,validation);
  }
  return error(Status::Ok,ESP_OK,checkpoint::Status::Ok);
}

Result store(const uint8_t *bytes,size_t length,
             const Meter &configured,uint32_t epoch) {
  if(!bytes || length!=checkpoint::kSize || epoch==0)
    return error(Status::InvalidArgument);
  char key[16]{};
  if(!make_key(configured,key)) return error(Status::InvalidArgument);
  Session candidate{};
  const auto validity=checkpoint::restore(candidate,bytes,length,
                                           configured,epoch);
  if(validity!=checkpoint::Status::Ok)
    return error(Status::CheckpointRejected,ESP_OK,validity);
  // Never auto-provision missing state, even through NVS_READWRITE.
  // A reviewed provisioning step is required before this writer can run.
  Session previous{};
  const Result loaded=load(previous,configured,epoch);
  if(loaded.status!=Status::Ok) return loaded;
  if(candidate.state.highest_counter<=previous.state.highest_counter)
    return error(Status::StaleRecord,ESP_OK,checkpoint::Status::DuplicateOrStale);

  nvs_handle_t handle{};
  const esp_err_t opened=nvs_open(kNamespace,NVS_READWRITE,&handle);
  if(opened!=ESP_OK) return error(Status::OpenFailed,opened);
  const esp_err_t set=nvs_set_blob(handle,key,bytes,length);
  if(set!=ESP_OK) {
    nvs_close(handle);
    return error(Status::SetFailed,set);
  }
  const esp_err_t committed=nvs_commit(handle);
  nvs_close(handle);
  if(committed!=ESP_OK) return error(Status::CommitFailed,committed);
  // Readback verifies the API-visible record, NOT physical anti-rollback
  // or guaranteed persistence across sudden power loss on real hardware.
  Session check{};
  const auto reread=load(check,configured,epoch);
  if(reread.status!=Status::Ok)
    return error(Status::ReadbackFailed,reread.esp_error,
                 reread.checkpoint_status);
  if(check.state.highest_counter!=candidate.state.highest_counter)
    return error(Status::ReadbackFailed);
  return error(Status::Ok,ESP_OK,checkpoint::Status::Ok);
}

const char *backend_name() {
  // Volatile function-pointer anchors keep BOTH real NVS load/store
  // implementations in the ESP-IDF link without EVER invoking them.
  using LoadFn=Result (*)(Session &,const Meter &,uint32_t);
  using StoreFn=Result (*)(const uint8_t *,size_t,const Meter &,uint32_t);
  static LoadFn volatile linked_load=&load;
  static StoreFn volatile linked_store=&store;
  (void) linked_load;
  (void) linked_store;
  return "NVS probe linked; inactive";
}
}}} // namespace xmq_portable::kamwater::nvs_probe
