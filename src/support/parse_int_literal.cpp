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

namespace pycc {
namespace support {

bool ParseIntLiteralStrict(std::string_view sv, int& out_val, std::string* err) {
  // Trim leading spaces
  size_t i = 0;
  while (i < sv.size() && std::isspace(static_cast<unsigned char>(sv[i]))) ++i;
  if (i) sv.remove_prefix(i);

  bool neg = false;
  if (!sv.empty() && (sv[0] == '+' || sv[0] == '-')) {
    neg = (sv[0] == '-');
    sv.remove_prefix(1);
  }
  if (sv.empty() || !std::isdigit(static_cast<unsigned char>(sv[0]))) {
    if (err) *err = "invalid integer literal";
    return false;
  }
  long long val = 0;
  for (char c : sv) {
    if (std::isspace(static_cast<unsigned char>(c))) break;
    if (!std::isdigit(static_cast<unsigned char>(c))) {
      if (err) *err = "invalid character in integer literal";
      return false;
    }
    val = val * 10 + (c - '0');
    if (val > std::numeric_limits<int>::max()) {
      if (err) *err = "integer overflow";
      return false;
    }
  }
  out_val = static_cast<int>(neg ? -val : val);
  return true;
}

}  // namespace support
}  // namespace pycc

