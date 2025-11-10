/***
 * Name: pycc::support::ConsumeSign
 * Purpose: Consume leading '+' or '-' and set is_negative.
 * Inputs: text (by ref), is_negative (by ref)
 * Outputs: is_negative set; text advanced by one if sign found; returns true always.
 */
#include "pycc/support/parse_util.h"

#include <string_view>

namespace pycc {
namespace support {

auto ConsumeSign(std::string_view& text, bool& is_negative) -> bool {
  is_negative = false;
  if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
    is_negative = (text[0] == '-');
    text.remove_prefix(1);
  }
  return true;
}

}  // namespace support
}  // namespace pycc

