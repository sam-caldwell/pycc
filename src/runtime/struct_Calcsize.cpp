/**
 * @file
 * @brief struct_calcsize_impl: compute size from parsed struct items.
 */
#include "runtime/detail/StructHandlers.h"
#include <cstdint>

namespace pycc::rt::detail {

int32_t struct_calcsize_impl(const std::vector<StructItem>& items) {
  std::size_t need = 0;
  for (const auto& it : items) {
    int w = (it.code=='f' || it.code=='i' || it.code=='I') ? 4 : 1;
    need += static_cast<std::size_t>(it.count) * static_cast<std::size_t>(w);
  }
  return static_cast<int32_t>(need);
}

} // namespace pycc::rt::detail

