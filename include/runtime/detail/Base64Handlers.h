/**
 * @file
 * @brief Base64 encode/decode helpers for runtime thin wrappers.
 */
#pragma once

#include <cstddef>
#include <string>

namespace pycc::rt::detail {

// Decode base64 bytes (ignores ASCII whitespace). Appends decoded bytes to out.
// Padding '=' is handled; invalid characters stop decoding conservatively.
void base64_decode_bytes(const unsigned char* data, std::size_t len, std::string& out);

} // namespace pycc::rt::detail

