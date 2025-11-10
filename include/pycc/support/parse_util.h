/***
 * Name: pycc::support (parse_util)
 * Purpose: Small helpers for parsing string_view inputs.
 * Inputs: std::string_view by reference, outputs via refs/pointers
 * Outputs: Mutated views and status booleans
 * Theory of Operation: Used to keep ParseIntLiteralStrict simple and readable.
 */
#pragma once

#include <string>
#include <string_view>

namespace pycc {
namespace support {

/*** TrimLeadingSpaces: Remove leading ASCII whitespace from view. */
void TrimLeadingSpaces(std::string_view& text);

/*** ConsumeSign: If + or -, consume and set is_negative accordingly. */
bool ConsumeSign(std::string_view& text, bool& is_negative);

/*** ParseDigitsStrict: Parse contiguous base-10 digits; stop at whitespace; set err on failure. */
bool ParseDigitsStrict(std::string_view text, long long& value, std::string* err);

}  // namespace support
}  // namespace pycc

