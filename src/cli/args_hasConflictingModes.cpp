#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {
    /***
     * Name: pycc::cli::detail::hasConflictingModes
     * Purpose: Validate mutually exclusive output modes.
     */
    bool hasConflictingModes(const Options &opts) {
        return opts.emitAssemblyOnly && opts.compileOnly;
    }
} // namespace pycc::cli::detail
