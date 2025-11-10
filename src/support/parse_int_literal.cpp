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
// NOLINTNEXTLINE(misc-include-cleaner) - include interface to ensure signature stays in sync
#include "pycc/support/parse.h"
#include "pycc/support/parse_util.h"

#include <cctype>
#include <cstddef>
#include <string>
#include <string_view>

namespace pycc::support {

auto ParseIntLiteralStrict(std::string_view text, int& out_val, std::string* err) -> bool {  // NOLINT(misc-use-internal-linkage,readability-non-const-parameter)
  TrimLeadingSpaces(text);

  bool is_negative = false;
  ConsumeSign(text, is_negative);
  if (text.empty() || std::isdigit(static_cast<unsigned char>(text[0])) == 0) {
    if (err != nullptr) {
      *err = "invalid integer literal";
    }
    return false;
  }
  long long value = 0;  // NOLINT(misc-const-correctness)
  if (!ParseDigitsStrict(text, value, err)) {
    return false;
  }
  out_val = static_cast<int>(is_negative ? -value : value);
  return true;
}

}  // namespace pycc::support
