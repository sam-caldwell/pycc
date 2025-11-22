#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {

/***
 * Name: pycc::cli::detail::parseColorValue
 * Purpose: Parse --color value into ColorMode with default.
 */
ColorMode parseColorValue(std::string_view value) {
    using enum pycc::cli::ColorMode;
    if (value == "always") { return Always; }
    if (value == "never") { return Never; }
    return Auto;
}

} // namespace pycc::cli::detail
