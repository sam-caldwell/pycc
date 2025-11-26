#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {
    /***
     * Name: pycc::cli::detail::isFlag
     * Purpose: Check if an argument exactly matches a flag.
     */
    bool isFlag(const std::string_view arg, const std::string_view flag) {
        return arg == flag;
    }
} // namespace pycc::cli::detail
