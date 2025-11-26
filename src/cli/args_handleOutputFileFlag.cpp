#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {
    /***
     * Name: pycc::cli::detail::handleOutputFileFlag
     * Purpose: Handle `-o <file>` output flag by consuming the next argument.
     */
    bool handleOutputFileFlag(int &idx, int argc, char **argv, Options &out) {
        // -o <file>
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        if (const std::string_view arg{argv[idx]}; !isFlag(arg, "-o")) { return false; }
        if (idx + 1 >= argc) { return false; }
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out.outputFile = argv[++idx];
        return true;
    }
} // namespace pycc::cli::detail
