#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {

/***
 * Name: pycc::cli::detail::parseAstLogValue
 * Purpose: Parse --ast-log value into AstLogMode with default.
 */
AstLogMode parseAstLogValue(std::string_view value) {
    using enum pycc::cli::AstLogMode;
    if (value == "after") { return After; }
    if (value == "both") { return Both; }
    return Before;
}

} // namespace pycc::cli::detail
