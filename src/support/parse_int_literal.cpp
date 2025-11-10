/***
 * Name: pycc::support::ParseIntLiteralStrict
 * Purpose: Parse a base-10 integer without throwing; fail on extra tokens.
 * Inputs:
 *   - sv: string view of the literal
 * Outputs:
 *   - out_val: parsed integer on success
 *   - err: optional error message on failure
 * Theory of Operation: Manual digit parsing with range check and whitespace trim.
 */
#include "pycc/support/parse.h"

#include <cctype>
#include <limits>

namespace pycc::support {

auto ParseIntLiteralStrict(std::string_view text, int& out_val, std::string* err) -> bool {
  // Trim leading spaces
  std::size_t index = 0;
  while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0) {
    ++index;
  }
  if (index > 0) {
    text.remove_prefix(index);
  }

  bool is_negative = false;
  if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
    is_negative = (text[0] == '-');
    text.remove_prefix(1);
  }
  if (text.empty() || std::isdigit(static_cast<unsigned char>(text[0])) == 0) {
    if (err != nullptr) {
      *err = "invalid integer literal";
    }
    return false;
  }
  long long value = 0;
  for (char ch : text) {
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      break;
    }
    if (std::isdigit(static_cast<unsigned char>(ch)) == 0) {
      if (err != nullptr) {
        *err = "invalid character in integer literal";
      }
      return false;
    }
    value = value * 10 + (ch - '0');
    if (value > std::numeric_limits<int>::max()) {
      if (err != nullptr) {
        *err = "integer overflow";
      }
      return false;
    }
  }
  out_val = static_cast<int>(is_negative ? -value : value);
  return true;
}

}  // namespace pycc::support
