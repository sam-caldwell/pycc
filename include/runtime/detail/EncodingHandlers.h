/**
 * @file
 * @brief Encoding/decoding helpers for utf-8/ascii.
 */
#pragma once

#include <cstddef>
#include <string>

namespace pycc::rt::detail {

// Decode utf-8 bytes; if invalid and errors=="replace", inserts U+FFFD sequences.
// Returns true on success; false if an exception should be raised by caller.
bool decode_utf8_bytes(const unsigned char* p, std::size_t nb, const char* errors, std::string& out_utf8);

// Decode ascii bytes; if non-ascii and errors=="replace", inserts '?' replacements.
bool decode_ascii_bytes(const unsigned char* p, std::size_t nb, const char* errors, std::string& out_ascii);

} // namespace pycc::rt::detail

