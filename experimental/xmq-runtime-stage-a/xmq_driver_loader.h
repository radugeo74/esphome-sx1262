#pragma once
// Stage A: strict, bounded, read-only loader for upstream .xmq driver files.
// No per-driver C++ and no ESPHome integration. The root is an explicit,
// trusted local directory; untrusted file names and symlinks fail closed.
#include "xmq_driver_registry.h"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xmq_runtime {

struct DriverLoadError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

inline bool safe_driver_basename(const std::string &name) {
  if (name.empty() || name.size()>96) return false;
  for (unsigned char c:name)
    if (!std::isalnum(c) && c!='_' && c!='-') return false;
  return true;
}

inline DriverRegistry load_registry_from_directory(
    const std::filesystem::path &directory, size_t max_drivers=128,
    size_t max_file_bytes=256*1024) {
  namespace fs=std::filesystem;
  if (!max_drivers || max_drivers>128 ||
      !max_file_bytes || max_file_bytes>256*1024)
    throw DriverLoadError("invalid XMQ loader resource limits");
  std::error_code ec;
  const auto root=fs::symlink_status(directory,ec);
  if (ec || !fs::is_directory(root) || fs::is_symlink(root))
    throw DriverLoadError("XMQ directory missing or unsafe");

  std::vector<fs::path> files;
  fs::directory_iterator it(directory,fs::directory_options::none,ec);
  if (ec) throw DriverLoadError("cannot enumerate XMQ directory");
  const fs::directory_iterator end;
  for (;it!=end;it.increment(ec)) {
    if (ec) throw DriverLoadError("cannot enumerate XMQ directory");
    const fs::path path=it->path();
    if (path.extension()!=".xmq") continue;
    const auto status=it->symlink_status(ec);
    if (ec || !fs::is_regular_file(status) || fs::is_symlink(status))
      throw DriverLoadError("XMQ entry is not a regular file");
    if (!safe_driver_basename(path.stem().string()))
      throw DriverLoadError("unsafe XMQ filename");
    if (files.size()>=max_drivers)
      throw DriverLoadError("too many XMQ driver files");
    files.push_back(path);
  }
  if (ec) throw DriverLoadError("XMQ directory enumeration error");
  if (files.empty()) throw DriverLoadError("no XMQ drivers");
  std::sort(files.begin(),files.end());

  std::vector<Driver> drivers;
  drivers.reserve(files.size());
  for (const auto &path:files) {
    const auto size=fs::file_size(path,ec);
    if (ec || size>max_file_bytes)
      throw DriverLoadError("XMQ driver exceeds size limit");
    std::ifstream stream(path,std::ios::binary);
    if (!stream) throw DriverLoadError("cannot read XMQ driver file");
    std::string source{std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>()};
    if (!stream.eof() && stream.bad())
      throw DriverLoadError("failed reading XMQ driver");
    if (source.size()!=size)
      throw DriverLoadError("XMQ driver changed during read");
    Driver driver=load_driver(source);
    // Never load a driver from an alias whose identity differs from the
    // filename. This avoids unexpected driver substitution on updates.
    if (driver.name!=path.stem().string())
      throw DriverLoadError("XMQ driver name differs from filename");
    drivers.push_back(std::move(driver));
  }
  return DriverRegistry(std::move(drivers));
}

} // namespace xmq_runtime
