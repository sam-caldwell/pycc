#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {
    /***
     * Name: pycc::cli::detail::isUnknownOptionArg
     * Purpose: Detect unsupported option-like arguments that start with '-'.
     */
    bool isUnknownOptionArg(const std::string_view arg) {
        return !arg.empty() && arg[0] == '-';
    }
} // namespace pycc::cli::detail
