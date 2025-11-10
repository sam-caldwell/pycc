/***
 * Name: pycc::support::ParseDigitsStrict
 * Purpose: Parse contiguous base-10 digits; stop at whitespace; report errors.
 * Inputs: text view, out value, optional error string pointer
 * Outputs: value and status; err set on failure
 */
#include "pycc/support/parse_util.h"

#include <cctype>
#include <limits>
#include <string>
#include <string_view>

namespace pycc {
namespace support {

auto ParseDigitsStrict(std::string_view text, long long& value, std::string* err) -> bool {
  value = 0;
  constexpr int kBase10 = 10;
  constexpr char kZeroChar = '0';
  bool is_success = true;
  std::string local_err;
  for (const char digit_char : text) {
    if (std::isspace(static_cast<unsigned char>(digit_char)) != 0) {
      break;
    }
    if (std::isdigit(static_cast<unsigned char>(digit_char)) == 0) {
      local_err = "invalid character in integer literal";
      is_success = false;
      break;
    }
    value = (value * kBase10) + (digit_char - kZeroChar);
    if (value > std::numeric_limits<int>::max()) {
      local_err = "integer overflow";
      is_success = false;
      break;
    }
  }
  if (!is_success) {
    if (err != nullptr) {
      *err = local_err;
    }
    return false;
  }
  return true;
}

}  // namespace support
}  // namespace pycc
