#pragma once
// Stage A: platform-independent loader for original XMQ source text.
// Names identify source entries. A caller can read from a host directory,
// firmware flash, or another storage adapter without changing XMQ semantics.
// This is NOT a flash filesystem implementation, no hot-reload, no RF/AES.
#include "xmq_driver_registry.h"
#include <cctype>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xmq_runtime {

struct SourceRegistryError : std::runtime_error {
  using std::runtime_error::runtime_error;
};

using XmqSourceReader = std::function<std::string(const std::string &)>;

inline bool valid_xmq_source_name(const std::string &name) {
  if (name.empty() || name.size()>96) return false;
  for (unsigned char c : name) {
    if (!std::isalnum(c) && c!='_' && c!='-') return false;
  }
  return true;
}

// Fail closed: rejects duplicate source names and mismatched driver.name.
// Limits apply to EACH payload, total supplied sources, and combined text.
// The caller provides source discovery and trusted, bounded storage reads.
inline DriverRegistry load_registry_from_reader(
    const std::vector<std::string> &names,
    const XmqSourceReader &read,
    size_t max_drivers=128,
    size_t max_source_bytes=256*1024,
    size_t max_total_bytes=1024*1024) {
  if (names.empty() || names.size()>128 || !read || max_drivers==0 ||
      max_drivers>128 || names.size()>max_drivers ||
      max_source_bytes==0 || max_source_bytes>256*1024 ||
      max_total_bytes==0 || max_total_bytes>1024*1024)
    throw SourceRegistryError("invalid XMQ source registry limits");
  std::set<std::string> unique;
  std::vector<Driver> parsed;
  parsed.reserve(names.size());
  size_t total=0;
  for (const auto &name : names) {
    if (!valid_xmq_source_name(name) || !unique.insert(name).second)
      throw SourceRegistryError("invalid or duplicate XMQ source identifier");
    const std::string source=read(name);
    if (source.empty() || source.size()>max_source_bytes ||
        source.size()>max_total_bytes-total)
      throw SourceRegistryError("XMQ source exceeds limits or is empty");
    total+=source.size();
    Driver driver=load_driver(source);
    if (driver.name!=name)
      throw SourceRegistryError("driver identity differs from XMQ source name");
    parsed.push_back(std::move(driver));
  }
  return DriverRegistry(std::move(parsed));
}

} // namespace xmq_runtime
