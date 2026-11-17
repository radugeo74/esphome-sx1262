#pragma once
// Stage A: read-only ESP-IDF VFS / mounted-flash adapter.
// Uses standard C stdio available to ESP-IDF VFS after a filesystem has
// ALREADY been mounted by the caller. This file does not mount partitions,
// format flash, update files, perform radio work, or publish HA sensors.
// Host tests use the same stdio calls; no ESP-IDF dependency is required.
//
// This is intentionally a trusted local mount adapter, not a sandbox for
// hostile host symlinks; callers control the mount root.
#include "xmq_source_registry.h"
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xmq_runtime {

struct FlashSourceError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

class MountedXmqFiles {
 public:
  // ESP32-specific conservative limits for the current experimental board.
  // These are not RAM-use measurements or proof of ESP32 viability.
  static constexpr size_t kDefaultMaxFile = 16*1024;
  static constexpr size_t kDefaultMaxTotal = 32*1024;
  static constexpr size_t kDefaultMaxDrivers = 8;

  explicit MountedXmqFiles(std::string mount_root,
                           size_t max_file=kDefaultMaxFile)
      : mount_root_(std::move(mount_root)), max_file_(max_file) {
    if (mount_root_.empty() || mount_root_[0]!='/' ||
        mount_root_.size()>128 || max_file_==0 ||
        max_file_>256*1024 || mount_root_.find("..")!=std::string::npos)
      throw FlashSourceError("invalid trusted flash mount root or limit");
    while (mount_root_.size()>1 && mount_root_.back()=='/')
      mount_root_.pop_back();
  }

  std::string read(const std::string &name) const {
    if (!valid_xmq_source_name(name))
      throw FlashSourceError("unsafe XMQ flash driver name");
    const std::string file=mount_root_+"/"+name+".xmq";
    if (file.size()>256) throw FlashSourceError("XMQ file path limit");
    struct FileCloser {
      void operator()(std::FILE *file) const noexcept {
        if (file) (void)std::fclose(file);
      }
    };
    using FilePtr=std::unique_ptr<std::FILE,FileCloser>;
    FilePtr handle(std::fopen(file.c_str(),"rb"));
    if (!handle) throw FlashSourceError("XMQ file not readable on mounted VFS");

    std::string source;
    char block[256];
    while (true) {
      const size_t count=std::fread(block,1,sizeof(block),handle.get());
      if (count>max_file_-source.size())
        throw FlashSourceError("XMQ flash file exceeds size limit");
      source.append(block,count);
      if (count<sizeof(block)) {
        if (std::ferror(handle.get()))
          throw FlashSourceError("error while reading XMQ flash file");
        if (std::feof(handle.get())) break;
        throw FlashSourceError("incomplete XMQ flash read");
      }
    }
    if (source.empty()) throw FlashSourceError("empty XMQ flash file");
    return source;
  }

  // Call only AFTER mount succeeds, with an explicit list of known names.
  // No directory traversal/discovery is performed on the embedded target.
  DriverRegistry load(const std::vector<std::string> &names,
                      size_t max_drivers=kDefaultMaxDrivers,
                      size_t max_total=kDefaultMaxTotal) const {
    if (max_drivers==0 || max_drivers>kDefaultMaxDrivers ||
        max_total==0 || max_total>kDefaultMaxTotal)
      throw FlashSourceError("unsafe embedded XMQ resource limits");
    const XmqSourceReader callback=[this](const std::string &name){
      return read(name);
    };
    return load_registry_from_reader(names,callback,max_drivers,max_file_,max_total);
  }

 private:
  std::string mount_root_;
  size_t max_file_;
};

} // namespace xmq_runtime
