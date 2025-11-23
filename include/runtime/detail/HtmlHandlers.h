/**
 * @file
 * @brief HTML unescape implementation helper for runtime.
 */
#pragma once

#include <string>

namespace pycc::rt::detail {

void html_unescape_impl(const char* data, std::size_t n, std::string& out);

}

