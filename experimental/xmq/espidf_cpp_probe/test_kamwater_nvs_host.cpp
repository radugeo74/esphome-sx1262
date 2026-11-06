// HOST-ONLY test double for the REAL kamwater_nvs_checkpoint.cpp adapter.
// This is NOT evidence of real flash, power-cut atomicity or ESP32 runtime.
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "host_fake_nvs/nvs.h"
#include "main/kamwater_nvs_checkpoint.h"

namespace fake {
using Blob=std::vector<uint8_t>;
struct Handle {
  std::string name{};
  bool writable{false};
  std::map<std::string,Blob> pending{};
};
std::map<std::string,Blob> committed{};
std::map<nvs_handle_t,Handle> open_handles{};
nvs_handle_t next_handle=1;
bool fail_open=false,fail_read=false,fail_set=false,fail_commit=false;
bool corrupt_committed_blob=false;
std::string key(const std::string &name,const char *field) {
  return name+":"+field;
}
void reset_faults() {
  fail_open=false;fail_read=false;fail_set=false;fail_commit=false;
  corrupt_committed_blob=false;
}
} // namespace fake

esp_err_t nvs_open(const char *name,nvs_open_mode_t mode,
                   nvs_handle_t *out) {
  if(fake::fail_open) return ESP_FAIL;
  if(!name || !out) return ESP_FAIL;
  bool exists=false;
  for(const auto &entry:fake::committed)
    if(entry.first.compare(0,std::strlen(name)+1,
                           std::string(name)+":")==0) exists=true;
  if(mode==NVS_READONLY && !exists) return ESP_ERR_NVS_NOT_FOUND;
  const auto h=fake::next_handle++;
  fake::open_handles[h]={name,mode==NVS_READWRITE,{}};
  *out=h;
  return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,
                       void *out,size_t *length) {
  if(fake::fail_read) return ESP_FAIL;
  const auto handle=fake::open_handles.find(h);
  if(handle==fake::open_handles.end() || !key || !length) return ESP_FAIL;
  const auto found=fake::committed.find(fake::key(handle->second.name,key));
  if(found==fake::committed.end()) return ESP_ERR_NVS_NOT_FOUND;
  const auto &blob=found->second;
  if(*length<blob.size()) {
    *length=blob.size();
    return ESP_ERR_NVS_INVALID_LENGTH;
  }
  if(out) std::memcpy(out,blob.data(),blob.size());
  *length=blob.size();
  return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,
                       const void *bytes,size_t count) {
  if(fake::fail_set) return ESP_FAIL;
  const auto found=fake::open_handles.find(h);
  if(found==fake::open_handles.end() || !found->second.writable ||
     !key || !bytes) return ESP_FAIL;
  const auto *ptr=static_cast<const uint8_t *>(bytes);
  found->second.pending[key]=fake::Blob(ptr,ptr+count);
  return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) {
  if(fake::fail_commit) return ESP_FAIL;
  const auto found=fake::open_handles.find(h);
  if(found==fake::open_handles.end() || !found->second.writable)
    return ESP_FAIL;
  for(const auto &entry:found->second.pending) {
    auto blob=entry.second;
    if(fake::corrupt_committed_blob && !blob.empty()) blob.back()^=1;
    fake::committed[fake::key(found->second.name,entry.first.c_str())]
        =std::move(blob);
  }
  found->second.pending.clear();
  return ESP_OK;
}
void nvs_close(nvs_handle_t h) {
  fake::open_handles.erase(h);
}

int main() {
  namespace ck=xmq_portable::kamwater::checkpoint;
  namespace nvs=xmq_portable::kamwater::nvs_probe;
  const nvs::Meter meter{0x2C37u,53820306u,0x3Cu,0x16u};
  const nvs::Meter other{0x2C37u,53820307u,0x3Cu,0x16u};
  constexpr uint32_t epoch=7;
  char key[16]{};
  if(!nvs::make_key(meter,key) ||
     std::string(key)!="kw2C3753820306" ||
     std::strlen(key)>15 || std::strlen(nvs::kNamespace)>15)
    return 1;
  auto invalid=meter;invalid.id=100000000u;
  if(nvs::make_key(invalid,key)) return 2;
  if(!nvs::make_key(meter,key)) return 3;
  const auto storage_key=fake::key(nvs::kNamespace,key);

  nvs::Session loaded{};
  if(nvs::load(loaded,meter,epoch).status!=nvs::Status::Missing ||
     loaded.ready) return 4;
  ck::State baseline{true,meter,0x000E578Bu};
  ck::Bytes encoded{};
  if(!ck::encode(baseline,epoch,encoded)) return 5;
  // No accidental first-use provisioning when the NVS key is missing.
  if(nvs::store(encoded.data(),encoded.size(),meter,epoch).status!=
       nvs::Status::Missing || !fake::committed.empty())
    return 6;
  // Simulated independently authorized provisioning (not adapter behavior).
  fake::committed[storage_key]={encoded.begin(),encoded.end()};
  if(nvs::load(loaded,meter,epoch).status!=nvs::Status::Ok ||
     !loaded.ready || loaded.state.highest_counter!=0x000E578Bu)
    return 7;
  if(nvs::load(loaded,other,epoch).status!=nvs::Status::Missing ||
     loaded.ready) return 8;
  if(nvs::load(loaded,meter,epoch+1).status!=
       nvs::Status::CheckpointRejected || loaded.ready)
    return 9;
  auto corrupted=fake::committed[storage_key];
  corrupted[20]^=1;
  fake::committed[storage_key]=corrupted;
  const auto bad_crc=nvs::load(loaded,meter,epoch);
  if(bad_crc.status!=nvs::Status::CheckpointRejected ||
     bad_crc.checkpoint_status!=ck::Status::Corrupt || loaded.ready)
    return 10;
  fake::committed[storage_key]={encoded.begin(),encoded.end()};
  fake::committed[storage_key].pop_back();
  if(nvs::load(loaded,meter,epoch).status!=
       nvs::Status::CheckpointRejected || loaded.ready)
    return 11;
  fake::committed[storage_key]={encoded.begin(),encoded.end()};

  ck::State stale_state{true,meter,0x000E578Au};
  ck::Bytes stale{};
  if(!ck::encode(stale_state,epoch,stale) ||
     nvs::store(stale.data(),stale.size(),meter,epoch).status!=
        nvs::Status::StaleRecord) return 12;
  if(nvs::store(encoded.data(),encoded.size(),other,epoch).status!=
       nvs::Status::CheckpointRejected) return 13;
  if(nvs::store(encoded.data(),encoded.size(),meter,epoch+1).status!=
       nvs::Status::CheckpointRejected) return 14;

  ck::State next{true,meter,0x000E578Cu};
  ck::Bytes higher{};
  if(!ck::encode(next,epoch,higher) ||
     nvs::store(higher.data(),higher.size(),meter,epoch).status!=
        nvs::Status::Ok ||
     nvs::load(loaded,meter,epoch).status!=nvs::Status::Ok ||
     loaded.state.highest_counter!=0x000E578Cu)
    return 15;
  if(nvs::store(higher.data(),higher.size(),meter,epoch).status!=
       nvs::Status::StaleRecord) return 16;
  auto on_disk=fake::committed[storage_key];

  ck::State third{true,meter,0x000E578Du};
  ck::Bytes newer{};
  if(!ck::encode(third,epoch,newer)) return 17;
  fake::fail_set=true;
  if(nvs::store(newer.data(),newer.size(),meter,epoch).status!=
       nvs::Status::SetFailed ||
     fake::committed[storage_key]!=on_disk) return 18;
  fake::reset_faults();
  fake::fail_commit=true;
  if(nvs::store(newer.data(),newer.size(),meter,epoch).status!=
       nvs::Status::CommitFailed ||
     fake::committed[storage_key]!=on_disk) return 19;
  fake::reset_faults();
  fake::fail_read=true;
  if(nvs::load(loaded,meter,epoch).status!=nvs::Status::ReadFailed ||
     loaded.ready ||
     nvs::store(newer.data(),newer.size(),meter,epoch).status!=
        nvs::Status::ReadFailed) return 20;
  fake::reset_faults();
  fake::fail_open=true;
  if(nvs::load(loaded,meter,epoch).status!=nvs::Status::OpenFailed ||
     loaded.ready) return 21;
  fake::reset_faults();
  fake::corrupt_committed_blob=true;
  if(nvs::store(newer.data(),newer.size(),meter,epoch).status!=
       nvs::Status::ReadbackFailed) return 22;
  fake::reset_faults();
  if(nvs::load(loaded,meter,epoch).status!=
       nvs::Status::CheckpointRejected || loaded.ready)
    return 23;
  if(!fake::open_handles.empty()) return 24;
  std::cout<<"PASS fake-NVS host adapter: restore, meter/epoch, "
              "monotonic commit, corrupt/truncated record, set/commit/"
              "readback failures, no auto-provision. NOT real NVS flash.\n";
  return 0;
}
