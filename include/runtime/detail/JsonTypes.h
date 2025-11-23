/**
 * @file
 * @brief Shared JSON dumping types for runtime helpers.
 */
#pragma once

#include <string>

namespace pycc::rt {

struct DumpOpts {
  int indent{0};
  bool ensureAscii{false};
  const char* sepItem{nullptr};
  const char* sepKv{nullptr};
  bool sortKeys{false};
};

using DumpRecFn = void(*)(void* obj, std::string& out, const DumpOpts& opts, int depth);

} // namespace pycc::rt

