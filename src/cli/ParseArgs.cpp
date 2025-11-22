#include "cli/ParseArgs.h"
#include "cli/ColorMode.h"
#include "cli/Options.h"
#include "cli/ParseArgsInternals.h"
#include <iostream>

namespace pycc::cli {
    /***
     * Name: pycc::cli::ParseArgs
     * Purpose: Minimal GCC-like CLI argument parser for pycc.
     */
    bool ParseArgs(const int argc, char **argv, Options &out) {
        for (int i = 1; i < argc; ++i) {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
            const std::string_view arg{argv[i]};
            // Handle -DNAME[=VALUE] style defines
            if (arg.rfind("-D", 0) == 0 && arg.size() > 2) {
                out.defines.emplace_back(std::string(arg.substr(2)));
                continue;
            }
            if (detail::isFlag(arg, "--")) {
                detail::collectRemainingAsInputs(i + 1, argc, argv, out);
                break;
            }
            if (detail::handleOutputFileFlag(i, argc, argv, out)) { continue; }
            if (detail::applySimpleBoolFlags(arg, out)) { continue; }
            if (detail::applyPrefixedOptions(arg, out)) { continue; }

            // Positional
            if (detail::isUnknownOptionArg(arg)) {
                std::cerr << "pycc: unknown option '" << arg << "'\n";
                return false;
            }
            out.inputs.emplace_back(std::string(arg));
        }

        if (detail::hasConflictingModes(out)) {
            std::cerr << "pycc: cannot use -S and -c together\n";
            return false;
        }

        return true;
    }
} // namespace pycc::cli
