/***
 * Name: pycc::frontend::ParseReturnInt
 * Purpose: Extract constant integer from a Python 'return N' statement.
 * Inputs:
 *   - source: Full module source text
 * Outputs:
 *   - out_value: Parsed integer if found
 *   - err: Error message if not found or invalid
 * Theory of Operation: Line-wise scan for a token 'return ' followed by an optional
 *   sign and digits; ignores other content. Intended as an MVP parser.
 */
#include "pycc/frontend/simple_return_int.h"

#include <string>
#include <string_view>

#include "pycc/support/parse.h"

namespace pycc::frontend {

auto ParseReturnInt(const std::string& source, int& out_value, std::string& err) -> bool {
  constexpr std::string_view kReturn = "return ";
  std::string_view sv(source);
  const std::size_t pos = sv.find(kReturn);
  if (pos == std::string_view::npos) {
    err = "no 'return <int>' statement found";
    return false;
  }
  sv.remove_prefix(pos + kReturn.size());
  const std::size_t line_end = sv.find_first_of("\n\r");
  if (line_end != std::string_view::npos) {
    sv = sv.substr(0, line_end);
  }
  int parsed_value = 0;
  if (!support::ParseIntLiteralStrict(sv, parsed_value, &err)) {
    if (err.empty()) {
      err = "invalid integer literal after return";
    }
    return false;
  }
  out_value = parsed_value;
  return true;
}

}  // namespace pycc::frontend
