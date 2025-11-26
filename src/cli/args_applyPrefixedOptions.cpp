#include "cli/ParseArgsInternals.h"

#include <algorithm>
#include <string>

namespace pycc::cli::detail {
    /***
     * Name: pycc::cli::detail::applyPrefixedOptions
     * Purpose: Parse and apply --key=value options like ast-log/color/diag-context.
     */
    bool applyPrefixedOptions(std::string_view arg, Options &out) {
        if (constexpr std::string_view astLogPrefix{"--ast-log="}; arg.rfind(astLogPrefix, 0) == 0) {
            out.astLog = parseAstLogValue(arg.substr(astLogPrefix.size()));
            return true;
        }

        if (constexpr std::string_view logPathPrefix{"--log-path="}; arg.rfind(logPathPrefix, 0) == 0) {
            out.logPath = std::string(arg.substr(logPathPrefix.size()));
            return true;
        }

        if (constexpr std::string_view colorPrefix{"--color="}; arg.rfind(colorPrefix, 0) == 0) {
            out.color = parseColorValue(arg.substr(colorPrefix.size()));
            return true;
        }

        if (constexpr std::string_view diagPrefix{"--diag-context="}; arg.rfind(diagPrefix, 0) == 0) {
            int numLines = 0;
            try { numLines = std::stoi(std::string(arg.substr(diagPrefix.size()))); } catch (...) { numLines = 0; }
            out.diagContext = std::max(numLines, 0);
            return true;
        }
        return false;
    }
} // namespace pycc::cli::detail
