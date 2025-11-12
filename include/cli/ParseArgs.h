#pragma once

#include <string>
#include <vector>

#include "ColorMode.h"
#include "ParseArgs.h"
#include "Options.h"

namespace pycc::cli {

    // Parse argv into Options. Returns false on fatal parse error.
    bool ParseArgs(int argc, char** argv, Options& out);

} // namespace pycc::cli