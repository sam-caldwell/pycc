/***
 * Name: pycc::support::TrimLeadingSpaces
 * Purpose: Remove leading ASCII whitespace from a string_view.
 * Inputs: text (by ref)
 * Outputs: text with prefix removed
 */
#include "pycc/support/parse_util.h"

#include <cctype>
#include <cstddef>
#include <string_view>

namespace pycc {
namespace support {

void TrimLeadingSpaces(std::string_view& text) {
  std::size_t index = 0;
  while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
    ++index;
  }
  if (index > 0) {
    text.remove_prefix(index);
  }
}

}  // namespace support
}  // namespace pycc

