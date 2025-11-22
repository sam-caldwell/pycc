#include "cli/ParseArgsInternals.h"

namespace pycc::cli::detail {

/***
 * Name: pycc::cli::detail::collectRemainingAsInputs
 * Purpose: Gather remaining argv entries as positional input paths.
 */
void collectRemainingAsInputs(std::size_t startIndex, int argc, char** argv, Options& out) {
    for (int j = static_cast<int>(startIndex); j < argc; ++j) {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        out.inputs.emplace_back(argv[j]);
    }
}

} // namespace pycc::cli::detail
