/***
 * Name: pycc::stages::detail::ParseReturnIntFromSource
 * Purpose: Fallback parse of 'return <int>' from source text.
 * Inputs: Source string
 * Outputs: Optional integer value
 * Theory of Operation: Locate "return ", parse int literal strictly.
 */
#include "pycc/stages/detail/ir_helpers.h"
#include "pycc/support/parse.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace pycc {
namespace stages {
namespace detail {

auto ParseReturnIntFromSource(const std::string& src) -> std::optional<int> {
  constexpr std::string_view kReturn = "return ";
  const std::size_t pos = src.find(kReturn);
  if (pos != std::string::npos) {
    const std::string_view view{src};
    int temp_value = 0;
    if (support::ParseIntLiteralStrict(view.substr(pos + kReturn.size()), temp_value)) {
      return temp_value;
    }
  }
  return std::nullopt;
}

}  // namespace detail
}  // namespace stages
}  // namespace pycc

