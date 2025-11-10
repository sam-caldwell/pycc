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

#include <cctype>
#include <string>
#include <string_view>

namespace pycc {
namespace frontend {

static bool ParseIntLiteral(std::string_view sv, int& out) {
  bool neg = false;
  size_t i = 0;
  if (i < sv.size() && (sv[i] == '+' || sv[i] == '-')) {
    neg = (sv[i] == '-');
    ++i;
  }
  if (i >= sv.size() || !std::isdigit(static_cast<unsigned char>(sv[i]))) return false;
  long val = 0;
  for (; i < sv.size() && std::isdigit(static_cast<unsigned char>(sv[i])); ++i) {
    val = val * 10 + (sv[i] - '0');
    if (val > 0x7fffffffL) return false;
  }
  // Trim trailing spaces
  while (i < sv.size() && std::isspace(static_cast<unsigned char>(sv[i]))) ++i;
  if (i != sv.size()) return false;  // extra tokens
  out = static_cast<int>(neg ? -val : val);
  return true;
}

bool ParseReturnInt(const std::string& source, int& out_value, std::string& err) {
  const std::string key = "return ";
  size_t start = source.find(key);
  if (start == std::string::npos) {
    err = "no 'return <int>' statement found";
    return false;
  }
  start += key.size();
  size_t end = source.find_first_of("\n\r", start);
  std::string_view tail(source.data() + start, (end == std::string::npos ? source.size() : end) - start);
  // Trim leading spaces
  size_t i = 0;
  while (i < tail.size() && std::isspace(static_cast<unsigned char>(tail[i]))) ++i;
  tail.remove_prefix(i);
  int val = 0;
  if (!ParseIntLiteral(tail, val)) {
    err = "invalid integer literal after return";
    return false;
  }
  out_value = val;
  return true;
}

}  // namespace frontend
}  // namespace pycc

