/***
 * Name: pycc::support::ParseIntLiteralStrict
 * Purpose: Parse a base-10 integer from a string view without throwing exceptions.
 * Inputs: Text containing optional sign and digits; optional error out
 * Outputs: Parsed integer via out_val; returns true on success
 * Theory of Operation: Validates characters and range; ignores trailing whitespace.
 */
#pragma once

#include <string>
#include <string_view>

namespace pycc {
namespace support {

bool ParseIntLiteralStrict(std::string_view text, int& out_val, std::string* err = nullptr);

}  // namespace support
}  // namespace pycc
